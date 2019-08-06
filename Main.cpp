// necessary headers
#include <Windows.h>
#include <WinUser.h>
#include <iostream>
#include <stdlib.h>
#include <processthreadsapi.h>
#include <strsafe.h>
#include <malloc.h>
#include <memory.h>
#include <wchar.h>
#include <string>
#include <math.h>
#include <vector>
#include <ctime>

// Direct2D API headers
#include <d2d1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>
#include <wincodec.h>

using namespace std;

// board variables
const float winMoveScore = 1000;
const int boardxspots = 7;
const int boardyspots = 6;
const int populationSize = 20;
const int gamesToRun = 200;
const float Kfactor = 150;

// creating a generic class of type interface 
template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease) {
	if (*ppInterfaceToRelease != NULL) {
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = NULL;
	}
}

#pragma comment(lib, "d2d1")

// defining custom windows message for checking to see if a player has won
#define WM_CHECK_CONNECT_FOUR (0x8010)
#define WM_PLAYER_ONE_MOVE (0x8011)
#define WM_PLAYER_TWO_MOVE (0x8012)

// defining macro for error handling
#ifndef Assert
#if defined( DEBUG ) || defined(_DEBUG)
#define Assert(b) do {if (!(b)) {OutputDebugStringA("Assert: " #b "\n");}} while(0)
#else
#define Assert(b)
#endif // DEBUG || _DEBUG
#endif

// defining macro for retrieving base address of module
#ifndef HINST_THISCOMPONENT
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)
#endif

// class that handles creating + discarding resources, message loop, rendering and the windows function
class GameWindow {
public:
	// constructor + destructor
	GameWindow();
	~GameWindow();

	// initalize null values for gamewindow variables
	HRESULT Initialize();
	HWND GetWinHandle();
	void RunMessageLoop();

	// checks if there is a space in a given row of the board
	int ValidatePlacement(int xpos);

	// keeps track of game progression
	int playerTurn;
	int movesPlayed;

private:
	//initialize resources
	HRESULT CreateDeviceIndependentResources();
	HRESULT CreateDeviceResources();

	// release device dependant resources
	void DiscardDeviceResources();

	// drawing
	HRESULT OnRender();

	// places counter
	int PlaceCounter(int x, int colour);

	// checks whether a player has won
	int CheckWin(int colour);

	// resize window
	void OnResize(UINT width, UINT height);

	// windows procedure
	static LRESULT CALLBACK WndProc(
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam
	);

	// Direct2D shizzle
	HWND m_hwnd;
	ID2D1Factory* m_pDirect2dFactory;
	ID2D1HwndRenderTarget* m_pRenderTarget;
	ID2D1SolidColorBrush* m_pRedCounterBrush;
	ID2D1SolidColorBrush* m_pBlueCounterBrush;
	ID2D1SolidColorBrush* m_pGridBrush;
	ID2D1SolidColorBrush* m_pEmptySpotBrush;

	// Connect4 board
	int LIVEGAMEBOARD[boardxspots][boardyspots];
};

GameWindow::GameWindow() :
	m_hwnd(NULL),
	m_pDirect2dFactory(NULL),
	m_pRenderTarget(NULL),
	m_pRedCounterBrush(NULL),
	m_pBlueCounterBrush(NULL),
	m_pGridBrush(NULL),
	m_pEmptySpotBrush(NULL),
	LIVEGAMEBOARD { {0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0}, { 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0}}
{
	playerTurn = 1;
	movesPlayed = 0;
}

GameWindow::~GameWindow() {
	SafeRelease(&m_pDirect2dFactory);
	SafeRelease(&m_pRenderTarget);
	SafeRelease(&m_pRedCounterBrush);
	SafeRelease(&m_pBlueCounterBrush);
	SafeRelease(&m_pGridBrush);
	SafeRelease(&m_pEmptySpotBrush);
}

// procedure to translate and dispatch messages
void GameWindow::RunMessageLoop() {
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

HWND GameWindow::GetWinHandle() {
	return m_hwnd;
}

int GameWindow::ValidatePlacement(int xpos) {
	for (int k = 0; k < boardyspots; k++) {
		if (LIVEGAMEBOARD[xpos][k] == 0) {
			return k;
		}
	}
	return -1;
}

// initialising the game
HRESULT GameWindow::Initialize() {
	// initialises independent resources (D2D factory)
	HRESULT hr;
	hr = CreateDeviceIndependentResources();

	if (SUCCEEDED(hr)) {

		//register the window class
		WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = GameWindow::WndProc;
		wcex.cbClsExtra = 0;
		wcex.hInstance = HINST_THISCOMPONENT;
		wcex.hbrBackground = NULL;
		wcex.lpszMenuName = NULL;
		wcex.hCursor = LoadCursor(NULL, IDI_APPLICATION);
		wcex.lpszClassName = L"AIC4";

		//registers the window class
		RegisterClassEx(&wcex);

		//uses the desktop dpi to generate window resolution / size
		RECT desktopArea;
		float width;
		float height;

		if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktopArea, 0) != ERROR) {
			width = static_cast<float>(desktopArea.right - desktopArea.left);
			height = static_cast<float>(desktopArea.bottom - desktopArea.top);

			//creates window
			m_hwnd = CreateWindow(
				L"AIC4",
				L"AI Connect 4",
				WS_OVERLAPPEDWINDOW,
				0,
				0,
				static_cast<UINT>(width),
				static_cast<UINT>(height),
				NULL,
				NULL,
				HINST_THISCOMPONENT,
				this
			);
			hr = m_hwnd ? S_OK : E_FAIL;
			if (SUCCEEDED(hr)) {
				ShowWindow(m_hwnd, SW_SHOWMAXIMIZED);
				UpdateWindow(m_hwnd);
			}
		}

	}
	return hr;
}

// WinMain (entry point) initialises an instance of the game and begins message loop
int WINAPI WinMain(_In_ HINSTANCE, _In_ HINSTANCE, _In_ LPSTR, _In_ int) {
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
	if (SUCCEEDED(CoInitialize(NULL))) {
		{
			GameWindow game;
			if (SUCCEEDED(game.Initialize())) {
				game.RunMessageLoop();
			}
		}
		CoUninitialize();
	}

	return 0;
}

// CREATING DIRECT2D RESOURCES:

// creates the D2D factory for the current instance of the game
HRESULT GameWindow::CreateDeviceIndependentResources() {
	HRESULT hr = S_OK;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pDirect2dFactory);
	return hr;
}

// creates render target and brushes for the current instance of the game
HRESULT GameWindow::CreateDeviceResources() {
	HRESULT hr = S_OK;
	if (!m_pRenderTarget) { // checks if render target already exists
		RECT rc;
		GetClientRect(m_hwnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

		// create render target
		hr = m_pDirect2dFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hwnd, size),
			&m_pRenderTarget);

		// create brushes
		if (SUCCEEDED(hr)) {
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Red),
				&m_pRedCounterBrush);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Blue),
				&m_pBlueCounterBrush);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::LightSlateGray),
				&m_pGridBrush);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pRenderTarget->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::GhostWhite),
				&m_pEmptySpotBrush);
		}
	}
	return hr;
}

// method to terminate resources
void GameWindow::DiscardDeviceResources() {
	SafeRelease(&m_pRenderTarget);
	SafeRelease(&m_pRedCounterBrush);
	SafeRelease(&m_pBlueCounterBrush);
	SafeRelease(&m_pGridBrush);
	SafeRelease(&m_pEmptySpotBrush);
}

// WndProc handles window messages
LRESULT CALLBACK GameWindow::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	if (message == WM_CREATE) {
		LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
		GameWindow* pGameWindow = (GameWindow*)pcs->lpCreateParams;
		::SetWindowLongPtrW(hwnd, GWLP_USERDATA, PtrToUlong(pGameWindow));
		result = 1;
		pGameWindow->playerTurn = 1;
	}
	else {
		GameWindow* pGameWindow = reinterpret_cast<GameWindow*>(static_cast<LONG_PTR>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA)));
		bool wasHandled = false;

		if (pGameWindow) {
			switch (message) {
			case WM_KEYUP:
				// take keyboard input
			{
				if (wParam > 48 && wParam < 56 && pGameWindow->playerTurn == 1) {
					wasHandled = true;
					int convertedInput = (int)wParam - 49;
					int validatedPlacement = pGameWindow->PlaceCounter(convertedInput, 1);
					if (validatedPlacement == 0) {
						pGameWindow->playerTurn = 2;
						InvalidateRect(hwnd, NULL, FALSE);

						// INSERT SEND MESSAGE MAGIC HERE
					}
				}
				PostMessage(hwnd, WM_CHECK_CONNECT_FOUR, wParam, lParam);
				bool wasHandled = true;
				result = 0;
			}
			break;

			case WM_CHECK_CONNECT_FOUR:
				// checks whether a winning move has been made
			{
				int checkRed = pGameWindow->CheckWin(1);
				int checkBlue = pGameWindow->CheckWin(2);

				if (checkBlue != 0 || checkRed != 0) {
					pGameWindow->playerTurn = 0;
				}
				result = 0;
				wasHandled = true;
			}
			break;

			case WM_PLAYER_TWO_MOVE:
			{
				// INSERT RECIEVING MAGIC HERE


				int theirMove = 0;
				pGameWindow->PlaceCounter(theirMove, 2);
				pGameWindow->playerTurn = 1;
				InvalidateRect(hwnd, NULL, FALSE);
				bool bloop = PostMessage(hwnd, WM_CHECK_CONNECT_FOUR, wParam, lParam);
				result = 0;
				wasHandled = true;
			}
			break;

			case WM_SIZE:
			{
				// resize window
				UINT width = LOWORD(lParam);
				UINT height = HIWORD(lParam);
				pGameWindow->OnResize(width, height);
				result = 0;
				wasHandled = true;
			}
			break;

			case WM_DISPLAYCHANGE:
			{
				// sets an area of the window to be redrawn next update
				InvalidateRect(hwnd, NULL, FALSE);
				result = 0;
				wasHandled = true;
			}
			break;

			case WM_PAINT:
			{
				// draws on board
				pGameWindow->OnRender();
				ValidateRect(hwnd, NULL);

				// handles end of game procedures
				if (pGameWindow->playerTurn == 0) {
					Sleep(3000);
					PostQuitMessage(0);
				}
				result = 0;
				wasHandled = true;
			}
			break;

			case WM_DESTROY:
			{
				//closes window
				PostQuitMessage(0);
				result = 1;
				wasHandled = true;
			}
			break;
			}
		}
		// defwindowproc handles any messages that have not been handled by WndProc
		// to ensure every message is processed
		if (!wasHandled) {
			result = DefWindowProc(hwnd, message, wParam, lParam);
		}
	}

	return result;
}

// Places a counter on the board
int GameWindow::PlaceCounter(int newxpos, int colour) {
	for (int k = 0; k < boardyspots; k++) {
		if (LIVEGAMEBOARD[newxpos][k] == 0) {
			LIVEGAMEBOARD[newxpos][k] = colour;
			return 0;
		}
	}
	return 1;
}

// Checks the board to see if a given colour has created a line of 4
int GameWindow::CheckWin(int colour) {
	// horizontal 4 in a row check
	for (int i = 0; i < 4; i++) {
		for (int k = 0; k < 6; k++) {
			if (LIVEGAMEBOARD[i][k] == colour && LIVEGAMEBOARD[i + 1][k] == colour && LIVEGAMEBOARD[i + 2][k] == colour && LIVEGAMEBOARD[i + 3][k] == colour) {
				return colour;
			}
		}
	}

	// vertical 4 in a row check
	for (int i = 0; i < 7; i++) {
		for (int k = 0; k < 3; k++) {
			if (LIVEGAMEBOARD[i][k] == colour && LIVEGAMEBOARD[i][k + 1] == colour && LIVEGAMEBOARD[i][k + 2] == colour && LIVEGAMEBOARD[i][k + 3] == colour) {
				return colour;
			}
		}
	}

	// Diagonal Down 4 in a row check
	for (int i = 0; i < 4; i++) {
		for (int k = 0; k < 3; k++) {
			if (LIVEGAMEBOARD[i][k] == colour && LIVEGAMEBOARD[i + 1][k + 1] == colour && LIVEGAMEBOARD[i + 2][k + 2] == colour && LIVEGAMEBOARD[i + 3][k + 3] == colour) {
				return colour;
			}
		}
	}

	// Diagonal Up 4 in a row check
	for (int i = 0; i < 4; i++) {
		for (int k = 3; k < 6; k++) {
			if (LIVEGAMEBOARD[i][k] == colour && LIVEGAMEBOARD[i + 1][k - 1] == colour && LIVEGAMEBOARD[i + 2][k - 2] == colour && LIVEGAMEBOARD[i + 3][k - 3] == colour) {
				return colour;
			}
		}
	}

	return 0;
}

// Renders the Game Board and any counters
HRESULT GameWindow::OnRender() {
	HRESULT hr = S_OK;
	hr = CreateDeviceResources();

	// checks that the drawing resources exist before continuing
	if (SUCCEEDED(hr)) {
		m_pRenderTarget->BeginDraw();
		m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
		m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

		// get size of area to be drawn on
		D2D1_SIZE_F rtsize = m_pRenderTarget->GetSize();
		float placementOffset = (rtsize.width / (2 * boardxspots));
		float counterRadius = ((rtsize.height - 100) / (2 * boardyspots));
		D2D1_ELLIPSE counterPiece = D2D1::Ellipse(D2D1::Point2F(rtsize.width / (2 * boardxspots)), counterRadius, counterRadius);

		// draw spots
		for (int i = boardxspots; i > 0; i--) {
			for (int k = boardyspots; k > 0; k--) {
				D2D1_ELLIPSE counterPiece = D2D1::Ellipse(D2D1::Point2F((rtsize.width * i / boardxspots) - placementOffset, ((rtsize.height - 30) * k / boardyspots) - counterRadius), counterRadius, counterRadius);
				m_pRenderTarget->FillEllipse(counterPiece, m_pEmptySpotBrush);
				m_pRenderTarget->DrawEllipse(counterPiece, m_pGridBrush, 0.8f);
			}
		}

		for (int i = 0; i < boardxspots; i++) {
			for (int k = 0; k < boardyspots; k++) {
				if (LIVEGAMEBOARD[i][k] == 1) {
					D2D1_ELLIPSE counterPiece = D2D1::Ellipse(D2D1::Point2F((rtsize.width * (i + 1) / boardxspots) - placementOffset, ((rtsize.height - 30) * (boardyspots - k) / boardyspots) - counterRadius), counterRadius, counterRadius);
					m_pRenderTarget->FillEllipse(counterPiece, m_pRedCounterBrush);
					m_pRenderTarget->DrawEllipse(counterPiece, m_pGridBrush, 0.8f);
				}
				else if (LIVEGAMEBOARD[i][k] == 2) {
					D2D1_ELLIPSE counterPiece = D2D1::Ellipse(D2D1::Point2F((rtsize.width * (i + 1) / boardxspots) - placementOffset, ((rtsize.height - 30) * (boardyspots - k) / boardyspots) - counterRadius), counterRadius, counterRadius);
					m_pRenderTarget->FillEllipse(counterPiece, m_pBlueCounterBrush);
					m_pRenderTarget->DrawEllipse(counterPiece, m_pGridBrush, 0.8f);
				}
			}
		}

		hr = m_pRenderTarget->EndDraw();
	}

	if (hr == D2DERR_RECREATE_TARGET) {
		hr = S_OK;
		DiscardDeviceResources();
	}

	return hr;
}

void GameWindow::OnResize(UINT width, UINT height) {
	if (m_pRenderTarget) {
		m_pRenderTarget->Resize(D2D1::SizeU(width, height));
	}
}
