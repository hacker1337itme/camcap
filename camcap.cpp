// WebcamApp.cpp
#include <windows.h>
#include <mfapi.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <commdlg.h>
#include <ctime>
#include <string>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

IMFSourceReader* g_reader = nullptr;
HWND g_previewWnd;
HWND g_statusText;

void DrawTimestamp(HDC hdc, int width, int height)
{
    time_t now = time(0);
    tm local;
    localtime_s(&local, &now);

    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);

    SetTextColor(hdc, RGB(255,255,255));
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, 10, height - 30, buffer, strlen(buffer));
}

void SaveBitmapToFile(HBITMAP hBitmap)
{
    OPENFILENAMEA ofn = {0};
    char filename[MAX_PATH] = "photo.bmp";

    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Bitmap Files (*.bmp)\0*.bmp\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "bmp";

    if (!GetSaveFileNameA(&ofn))
        return;

    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPFILEHEADER bmfHeader = {0};
    BITMAPINFOHEADER bi = {0};

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;
    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    char* lpbitmap = (char*)GlobalLock(hDIB);

    HDC hdc = GetDC(NULL);
    GetDIBits(hdc, hBitmap, 0,
              (UINT)bmp.bmHeight,
              lpbitmap,
              (BITMAPINFO*)&bi,
              DIB_RGB_COLORS);

    HANDLE hFile = CreateFileA(filename,
                                GENERIC_WRITE,
                                0,
                                NULL,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

    DWORD dwSizeOfDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = dwSizeOfDIB;
    bmfHeader.bfType = 0x4D42;

    DWORD dwWritten;
    WriteFile(hFile, &bmfHeader, sizeof(bmfHeader), &dwWritten, NULL);
    WriteFile(hFile, &bi, sizeof(bi), &dwWritten, NULL);
    WriteFile(hFile, lpbitmap, dwBmpSize, &dwWritten, NULL);

    CloseHandle(hFile);
    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
    ReleaseDC(NULL, hdc);
}

void CaptureFrame()
{
    IMFSample* sample = nullptr;
    DWORD streamIndex, flags;
    LONGLONG timestamp;

    if (FAILED(g_reader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0, &streamIndex, &flags, &timestamp, &sample)))
        return;

    if (!sample) return;

    IMFMediaBuffer* buffer = nullptr;
    sample->ConvertToContiguousBuffer(&buffer);

    BYTE* data = nullptr;
    DWORD maxLen, curLen;
    buffer->Lock(&data, &maxLen, &curLen);

    // Assume 640x480 RGB for simplicity
    int width = 640;
    int height = 480;

    HDC hdc = GetDC(NULL);
    HBITMAP hBitmap = CreateBitmap(width, height, 1, 24, data);
    HDC memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hBitmap);

    DrawTimestamp(memDC, width, height);

    SaveBitmapToFile(hBitmap);

    DeleteDC(memDC);
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hdc);

    buffer->Unlock();
    buffer->Release();
    sample->Release();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1)
                CaptureFrame();
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    MFStartup(MF_VERSION);

    IMFAttributes* attr = nullptr;
    IMFActivate** devices = nullptr;
    UINT32 count = 0;

    MFCreateAttributes(&attr, 1);
    attr->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                  MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    MFEnumDeviceSources(attr, &devices, &count);

    IMFMediaSource* source = nullptr;
    devices[0]->ActivateObject(IID_PPV_ARGS(&source));
    MFCreateSourceReaderFromMediaSource(source, NULL, &g_reader);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "WebcamApp";
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("WebcamApp", "Webcam Photo App",
        WS_OVERLAPPEDWINDOW,
        200,200,800,600,
        NULL,NULL,hInstance,NULL);

    CreateWindow("BUTTON","Capture Photo",
        WS_VISIBLE|WS_CHILD,
        20,20,150,40,
        hwnd,(HMENU)1,hInstance,NULL);

    g_statusText = CreateWindow("STATIC","Camera Active",
        WS_VISIBLE|WS_CHILD,
        200,25,200,30,
        hwnd,NULL,hInstance,NULL);

    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while(GetMessage(&msg,NULL,0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_reader->Release();
    MFShutdown();
    return 0;
}
