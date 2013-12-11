// tetris.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "lambda_tetris.h"

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	enum kConfig {
		  MAX_AREA_W = 10
		, MAX_AREA_H = 25
		, PADDING_X  = 10
		, PADDING_Y  = 20
		, BLOCK_SIZE = 20
		, NEXT_AREA_SIZE=BLOCK_SIZE*6
		, WINDOW_W = 400
		, WINDOW_H = 600
	};

	struct Position {
		LONG x, y;
		Position& operator += (const Position& rhs) {
			x += rhs.x;
			y += rhs.y;
			return *this;
		}
		Position operator + (const Position& rhs) const {
			Position pos = *this;
			pos += rhs;
			return pos;
		}
	};
	struct Matrix {
		POINT x;
		POINT y;
	};
	struct Block {
		Position pos;
		COLORREF color;
	};
	class Mino {
	public:
		::std::vector<Block> blocks;
		Position pos;
	public:
		Mino(::std::initializer_list<Block> list) : blocks(list), pos(Position{ kConfig::MAX_AREA_W / 2, -4 }) {};

		int left() const {
			int x= ::std::numeric_limits<int>::max();
			::std::for_each(blocks.begin(), blocks.end(), [&](const Block& blk){ if( x > blk.pos.x ) x = blk.pos.x; });
			return x + pos.x;
		}
		int right() const {
			int x= ::std::numeric_limits<int>::min();
			::std::for_each(blocks.begin(), blocks.end(), [&](const Block& blk){ if( x < blk.pos.x ) x = blk.pos.x; });
			return x + pos.x;
		}
		int bottom() const {
			int y= ::std::numeric_limits<int>::min();
			::std::for_each(blocks.begin(), blocks.end(), [&](const Block& blk){ if( y < blk.pos.y ) y = blk.pos.y; });
			return y + pos.y;
		}
		void rotate(Matrix mtx) {
			for( auto& blk : blocks ) {
				blk.pos = { mtx.x.x * blk.pos.x + mtx.x.y * blk.pos.y, mtx.y.x * blk.pos.x + mtx.y.y * blk.pos.y };
			}
		}
	};
	struct FieldBlock : public Block {
		FieldBlock(const Mino* mino, const Block& blk) {
			pos = blk.pos + mino->pos;
			color = RGB(GetRValue(blk.color) / 2, GetGValue(blk.color) / 2, GetBValue(blk.color) / 2);
		}
	};
	struct Field {
		::std::vector<FieldBlock> line[kConfig::MAX_AREA_H];
	public:
		bool find(Position pos) {
			if( pos.y < 0 || pos.y >= MAX_AREA_H ) return false;
			const auto it = ::std::find_if(line[pos.y].begin(), line[pos.y].end()
				, [=](const FieldBlock& b){ return b.pos.x <= pos.x && b.pos.x >= pos.x; });
			return (it != ::std::end(line[pos.y]));
		}
		bool find(::std::shared_ptr<Mino> mino) {
			for( const auto& blk : mino->blocks ) {
				if( find(blk.pos+mino->pos) ) return true;
			}
			return false;
		}
		bool fix(::std::shared_ptr<Mino> mino) {
			bool over = false;
			for( const auto& blk : mino->blocks ) {
				if( mino->pos.y + blk.pos.y < 0 ) {
					over = true;
					continue;
				}
				assert(mino->pos.y + blk.pos.y < MAX_AREA_H);
				line[mino->pos.y + blk.pos.y].emplace_back(mino.get(), blk);
			}
			return over;
		}
		int checkLine() {
			int count=0;
			for( int i=0; i < kConfig::MAX_AREA_H; ++i ) {
				if( line[i].size() == kConfig::MAX_AREA_W ) {
					auto tmp(::std::move(line[i]));
					for( int j=i; j > 0; --j ) {
						line[j] = ::std::move(line[j-1]);
						::std::for_each(line[j].begin(), line[j].end(), [j](FieldBlock& blk){ blk.pos.y = j; });
					}
					line[0] = ::std::move(tmp);
					line[0].clear();
					++count;
				}
			}
			return count;
		}
	};

	struct Game {
		::std::shared_ptr<Mino> m_curr;
		::std::shared_ptr<Mino> m_next;
		Field m_field;
		::std::mt19937 m_rnd;
		float m_speed;
		float m_y;
		struct KeyState {
			DWORD hold;
			DWORD trigger;
		} m_keyState;

		Game() : m_curr(nullptr), m_next(nullptr), m_speed(0.1f), m_y(0.0f)
		{
			::std::random_device rd;
			m_rnd.seed(rd());

			m_curr = MakeMino();
			m_next = MakeMino();
		}

		enum {
			  BUTTON_UP		= 0x00
			, BUTTON_DOWN	= 0x01
			, BUTTON_LEFT	= 0x02
			, BUTTON_RIGHT	= 0x04
			, BUTTON_SPACE	= 0x08
		};
		DWORD UpdateKeyState()
		{
			DWORD key = 0;
			if( GetKeyState(VK_UP) < 0 ) key|=BUTTON_UP;
			if( GetKeyState(VK_DOWN) < 0 ) key|=BUTTON_DOWN;
			if( GetKeyState(VK_LEFT) < 0 ) key|=BUTTON_LEFT;
			if( GetKeyState(VK_RIGHT) < 0 ) key|=BUTTON_RIGHT;
			if( GetKeyState(VK_SPACE) < 0 ) key|=BUTTON_SPACE;

			m_keyState.trigger = (m_keyState.hold ^ key) & key;
			m_keyState.hold = key;
			return key;
		}
		bool IsFloor(::std::shared_ptr<Mino> mino)
		{
			const int bottom = mino->bottom();
			if( bottom >= kConfig::MAX_AREA_H-1 ) return true;

			for( const auto& blk : mino->blocks ) {
				const auto y = mino->pos.y + blk.pos.y + 1;
				const auto x = mino->pos.x + blk.pos.x;
				if( y < 0 || y >= MAX_AREA_H ) continue;
				const auto& line = m_field.line[y];
				const auto it = ::std::find_if(line.begin(), line.end(), [=](const FieldBlock& b){ return b.pos.x == x; });
				if( it != ::std::end(line) ) return true;
			}
			return false;
		}
		bool CheckMoveX(::std::shared_ptr<Mino> mino, Position& move)
		{
			if( move.x < 0 && ( mino->left() + move.x < 0 ) ) {
				move.x = 0;
				return true;
			}
			if( move.x > 0 && ( mino->right() + move.x >= MAX_AREA_W ) ) {
				move.x = 0;
				return true;
			}
			if( move.x ) {
				for( const auto& blk : mino->blocks ) {
					if( m_field.find({ mino->pos.x + blk.pos.x + move.x, mino->pos.y + blk.pos.y }) ) {
						move.x = 0;
						return true;
					}
				}
			}
			return false;
		}
		void Update()
		{
			UpdateKeyState();
			if( m_curr != nullptr )
			{
				m_y += m_speed;
				Position move{ 0, 0 };
				if( m_y > 1.0f ) {
					move.y = 1;
					m_y -= 1.0f;
				}

				if( m_keyState.trigger & BUTTON_LEFT ) {
					move.x -= 1;
				}
				if( m_keyState.trigger & BUTTON_RIGHT ) {
					move.x += 1;
				}
				if( m_keyState.hold & BUTTON_DOWN ) {
					move.y += 1;
				}
				if( m_keyState.trigger & BUTTON_SPACE ) {
					m_curr->rotate({ 0, 1, -1, 0 });
					if( m_curr->left() < 0 || m_curr->right() >= MAX_AREA_W || m_field.find(m_curr) ) {
						m_curr->rotate({ 0, -1, 1, 0 });
					}
				}

				CheckMoveX(m_curr, move);
				auto move_y = move.y;
				move.y = 0;
				m_curr->pos += move;
				for( int i=0; i < move_y; ++i ) {
					if( IsFloor(m_curr) ) {
						break;
					}
					m_curr->pos.y += 1;
				}

				if( IsFloor(m_curr) ) {
					if( m_field.fix(m_curr) ) {
						m_curr = nullptr;
					} else {
						m_curr = m_next;
						m_next = MakeMino();
					}
				}
			}

			m_field.checkLine();
		}
		::std::shared_ptr<Mino> MakeMino()
		{
			::std::uniform_int_distribution<int> dist(0, 6);
			switch( dist(m_rnd) )
			{
			case 0:
				{
					COLORREF c=RGB(0, 255, 255);
					return ::std::make_shared<Mino>(Mino{ { 0, -1, c }, { 0, 0, c }, { 0, 1, c }, { 0, 2, c } });
				}
				break;
			case 1:
				{
					COLORREF c=RGB(255, 255, 0);
					return ::std::make_shared<Mino>(Mino{ { 0, 0, c }, { 0, 1, c }, { 1, 0, c }, { 1, 1, c } });
				}
				break;
			case 2:
				{
					COLORREF c=RGB(0, 255, 0);
					return ::std::make_shared<Mino>(Mino{ { -1, 0, c }, { 0, 0, c }, { 0, 1, c }, { 1, 1, c } });
				}
				break;
			case 3:
				{
					COLORREF c=RGB(255, 0, 0);
					return ::std::make_shared<Mino>(Mino{ { -1, 1, c }, { 0, 1, c }, { 0, 0, c }, { 1, 0, c } });
				}
				break;
			case 4:
				{
					COLORREF c=RGB(255, 128, 0);
					return ::std::make_shared<Mino>(Mino{ { 0, -1, c }, { 0, 0, c }, { 0, 1, c }, { 1, 1, c } });
				}
				break;
			case 5:
				{
					COLORREF c=RGB(0, 0, 255);
					return ::std::make_shared<Mino>(Mino{ { 1, -1, c }, { 1, 0, c }, { 1, 1, c }, { 0, 1, c } });
				}
				break;
			case 6:
				{
					COLORREF c=RGB(255, 0, 255);
					return ::std::make_shared<Mino>(Mino{ { -1, 1, c }, { 0, 0, c }, { 0, 1, c }, { 1, 1, c } });
				}
				break;
			default:
				break;
			}
			return nullptr;
		}
	};

	struct Global {
		Game game;
		static Global& GetInstance() { static Global g; return g; }
	};

	auto WndProc =[](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
		DLGPROC About =static_cast<DLGPROC>([](HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) -> INT_PTR {
			UNREFERENCED_PARAMETER(lParam);
			switch( message )
			{
			case WM_INITDIALOG:
				return (INT_PTR)TRUE;

			case WM_COMMAND:
				if( LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL )
				{
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
				}
				break;
			}
			return (INT_PTR)FALSE;
		});

		static HDC hMemDC = nullptr;
		static HBITMAP hBitmap = nullptr;

		switch( message )
		{
		case WM_CREATE:
			{
				HDC hDC     = GetDC(hWnd);
				hMemDC      = CreateCompatibleDC(hDC);
				hBitmap     = CreateCompatibleBitmap(hDC, WINDOW_W, WINDOW_H);
				SelectObject(hMemDC, hBitmap);
				for( auto i : { DEFAULT_GUI_FONT, DC_PEN, DC_BRUSH } ) {
					SelectObject(hMemDC, GetStockObject(i));
				}
				ReleaseDC(hWnd, hDC);
			}
			break;
		case WM_COMMAND:
			{
				const int wmId    = LOWORD(wParam);
				switch( wmId )
				{
				case IDM_ABOUT:
					DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
					break;
				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;
				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
				}
			}
			break;
		case WM_PAINT:
			{
				Game game = Global::GetInstance().game;

				auto hFrame = CreateSolidBrush(RGB(0, 0, 0));
				auto DrawBlock = [=](HDC hdc, Position pos, COLORREF color) {
					RECT rc ={ pos.x, pos.y, pos.x + kConfig::BLOCK_SIZE, pos.y + kConfig::BLOCK_SIZE };
					auto hBrush = CreateSolidBrush(color);
					FillRect(hdc, &rc, hBrush);
					FrameRect(hdc, &rc, hFrame);
					DeleteObject(hBrush);
				};
				auto DrawMino = [=](::std::shared_ptr<Mino> mino, Position base) {
					if( mino == nullptr ) return;
					for( const Block& blk : mino->blocks ) {
						if( base.y + blk.pos.y < 0 ) continue;
						DrawBlock(hMemDC, { base.x + blk.pos.x*kConfig::BLOCK_SIZE, base.y + blk.pos.y*kConfig::BLOCK_SIZE }
						, blk.color);
					}
				};
				auto DrawField =[=]() {
					for( const auto& line : game.m_field.line ) {
						for( const FieldBlock& blk : line ) {
							DrawBlock(hMemDC, { blk.pos.x*kConfig::BLOCK_SIZE + kConfig::PADDING_X
								, blk.pos.y*kConfig::BLOCK_SIZE + kConfig::PADDING_Y }, blk.color);
						}
					}
				};
				auto DrawFrame = [=](RECT rc) {
					FrameRect(hMemDC, &rc, hFrame);
				};

				{
					RECT rc ={ 0, 0, WINDOW_W, WINDOW_H };
					FillRect(hMemDC, &rc, NULL);
				}

				DrawFrame({ PADDING_X, PADDING_Y, PADDING_X + BLOCK_SIZE*MAX_AREA_W, PADDING_Y + BLOCK_SIZE*MAX_AREA_H });

				{
					const LONG left = PADDING_X + BLOCK_SIZE*MAX_AREA_W + 40;
					const LONG top = PADDING_Y;
					DrawFrame({ left, top, left + NEXT_AREA_SIZE, top + NEXT_AREA_SIZE });
					Position base{ left + NEXT_AREA_SIZE/2 - BLOCK_SIZE/4, top + NEXT_AREA_SIZE/2 - BLOCK_SIZE };
					DrawMino(game.m_next, base);
				}

				if( game.m_curr != nullptr ) {
					Position base{ game.m_curr->pos.x*BLOCK_SIZE + kConfig::PADDING_X, game.m_curr->pos.y*BLOCK_SIZE + kConfig::PADDING_Y };
					DrawMino(game.m_curr, base);
				}
				DrawField();

				if( game.m_curr == nullptr ) {
					SetTextColor(hMemDC, RGB(255, 0, 0));
					TextOut(hMemDC, 80, WINDOW_H/2, _T("GAME OVER"), 9);
				}
				DeleteObject(hFrame);

				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);

				BitBlt(hdc, 0, 0, WINDOW_W, WINDOW_H, hMemDC, 0, 0, SRCCOPY);
				EndPaint(hWnd, &ps);
			}
			break;
		case WM_ERASEBKGND:
			return TRUE;
		case WM_DESTROY:
			DeleteObject(hBitmap);
			DeleteDC(hMemDC);
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return 0;
	};

	// アプリケーションの初期化を実行します:
	const int MAX_LOADSTRING = 100;
	TCHAR szTitle[MAX_LOADSTRING];					// タイトル バーのテキスト
	TCHAR szWindowClass[MAX_LOADSTRING];			// メイン ウィンドウ クラス名
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_TETRIS, szWindowClass, MAX_LOADSTRING);

	[&](){
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);

		wcex.style			= CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc	= WndProc;
		wcex.cbClsExtra		= 0;
		wcex.cbWndExtra		= 0;
		wcex.hInstance		= hInstance;
		wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TETRIS));
		wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_TETRIS);
		wcex.lpszClassName	= szWindowClass;
		wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		return RegisterClassEx(&wcex);
	}();

	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, WINDOW_W, WINDOW_H, NULL, NULL, hInstance, NULL);

	if( !hWnd ) {
		return FALSE;
	}

	auto TimerProc =[](HWND hWnd, UINT nMsg, UINT_PTR nIDEvent, DWORD dwTime) {
		Global::GetInstance().game.Update();
		InvalidateRect(hWnd, nullptr, TRUE);
	};
	SetTimer(hWnd, NULL, 1000 / 60, TimerProc);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	MSG msg;
	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TETRIS));

	// メイン メッセージ ループ:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}
