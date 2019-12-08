#include"scg.h"
#include<Windows.h>

// exposed
unsigned* scg_back_buffer = NULL;
int scg_window_width = 0;
int scg_window_height = 0;

static const TCHAR *window_title = _T("default window title");
static HWND window_handle = 0;
static WNDCLASS window_class = {0};
static ATOM window_class_atom = 0;
static HDC back_buffer_dc = 0;
static HBITMAP back_buffer_handle = 0;
static HBITMAP back_buffer_original_handle = 0;

int scg_create_window(int width, int height, const TCHAR* title, WNDPROC event_process)
{
	// classic window and cannot be resized
	DWORD window_style = WS_OVERLAPPEDWINDOW & (~WS_THICKFRAME);

    if (title)
        window_title = title;
	window_class.style = CS_BYTEALIGNWINDOW | CS_BYTEALIGNCLIENT;
	if (event_process)
		window_class.lpfnWndProc = event_process;
	else 
		window_class.lpfnWndProc = DefWindowProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = 0;
	window_class.hIcon = NULL;
	window_class.hCursor = LoadCursor(0, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = _T("ScgWndClass");
	window_class_atom = RegisterClass(&window_class);
	if (!window_class_atom)
		return -1;

	scg_window_width = width;
	scg_window_height = height;
	RECT tmp_rect = {0, 0, width, height};
	AdjustWindowRect(&tmp_rect, window_style, 0);
	// width and height plus window frame width height
	long tmp_window_width = tmp_rect.right - tmp_rect.left;
	long tmp_window_height = tmp_rect.bottom - tmp_rect.top;
	window_handle = CreateWindow(
		(LPCTSTR)window_class_atom,
        window_title,
		window_style,
		(GetSystemMetrics(SM_CXSCREEN) - tmp_window_width) / 2,
		(GetSystemMetrics(SM_CYSCREEN) - tmp_window_height) / 2,
		tmp_window_width,
		tmp_window_height,
		NULL,
		NULL,
		window_class.hInstance,
		NULL);
	if (!window_handle)
		return -1;

	HDC main_dc = GetDC(window_handle);
	back_buffer_dc = CreateCompatibleDC(main_dc);
	ReleaseDC(window_handle, main_dc);

	BITMAPINFO bmp_info = {{0}};
	bmp_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmp_info.bmiHeader.biWidth = scg_window_width;
	bmp_info.bmiHeader.biHeight = -scg_window_height;		// top_down dib section
	bmp_info.bmiHeader.biPlanes = 1;
	bmp_info.bmiHeader.biBitCount = 32;
	bmp_info.bmiHeader.biCompression = BI_RGB;
	bmp_info.bmiHeader.biSizeImage = scg_window_width * scg_window_height * 4;
	bmp_info.bmiHeader.biXPelsPerMeter = 2835;
	bmp_info.bmiHeader.biYPelsPerMeter = 2835;
	bmp_info.bmiHeader.biClrUsed = 0;
	bmp_info.bmiHeader.biClrImportant = 0;
	back_buffer_handle = CreateDIBSection(back_buffer_dc, &bmp_info, DIB_RGB_COLORS, (void**)&scg_back_buffer, 0, 0);
	if (!back_buffer_handle)
		return -1;

	// swap out the old buffer
	back_buffer_original_handle = (HBITMAP)SelectObject(back_buffer_dc, back_buffer_handle);

	ShowWindow(window_handle, SW_SHOW);
	SetForegroundWindow(window_handle);

	return 0;
}

void scg_close_window(void)
{
	if (back_buffer_dc) {
		if (back_buffer_original_handle) {
			SelectObject(back_buffer_dc, back_buffer_original_handle);
			DeleteObject(back_buffer_handle);
		}
		DeleteDC(back_buffer_dc);
	}

	if (window_handle)
		CloseWindow(window_handle);

	if (window_class.lpfnWndProc)
		UnregisterClass((LPCSTR)window_class_atom, 0);
}

void scg_refresh(void)
{
	HDC main_dc = GetDC(window_handle);
	BitBlt(main_dc, 0, 0, scg_window_width, scg_window_height, back_buffer_dc, 0, 0, SRCCOPY);
	ReleaseDC(window_handle, main_dc);
}

void scg_msg_dispatch(void)
{
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
		if (GetMessage(&msg, 0, 0, 0) == 0)
			break;
		DispatchMessage(&msg);
	}
}

