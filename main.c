#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

// Global variables for image data
unsigned char* image_data = NULL;
int img_width, img_height, img_channels;
HBITMAP hBitmap = NULL;

// Function to convert STB image data to Windows HBITMAP
HBITMAP CreateBitmapFromSTB(unsigned char* data, int width, int height, int channels) {
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24; // We'll force 24-bit RGB
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;
    bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrImportant = 0;

    // Create a memory DC — never SelectObject a bitmap into a screen DC
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    ReleaseDC(NULL, hdcScreen);

    if (!hdcMem || !hBmp) {
        if (hdcMem) DeleteDC(hdcMem);
        if (hBmp)   DeleteObject(hBmp);
        return NULL;
    }

    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);
    
    // Convert image data to 24-bit RGB if necessary
    int rowSize = width * 3;
    int stride = (rowSize + 3) & ~3; // Align to 4-byte boundary
    unsigned char* rgb_data = (unsigned char*)calloc(stride * height, 1);
    if (!rgb_data) {
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        return NULL;
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int src_idx = (y * width + x) * channels;
            int dst_idx = (y * stride) + (x * 3);
            if (channels >= 3) {
                // Windows DIB expects BGR order, but stb_image returns RGB
                rgb_data[dst_idx]     = data[src_idx + 2]; // B
                rgb_data[dst_idx + 1] = data[src_idx + 1]; // G
                rgb_data[dst_idx + 2] = data[src_idx];     // R
            } else {
                // Grayscale: copy the single channel to all three
                unsigned char gray = data[src_idx];
                rgb_data[dst_idx]     = gray;
                rgb_data[dst_idx + 1] = gray;
                rgb_data[dst_idx + 2] = gray;
            }
        }
    }

    SetDIBits(hdcMem, hBmp, 0, height, rgb_data, &bmi, DIB_RGB_COLORS);
    
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);
    free(rgb_data);
    
    return hBmp;
}

/* Global zoom factor */
float gZoom = 1.0f; // 1.0 = 100%

/* Scroll position and drag state */
int gScrollX = 0, gScrollY = 0;
BOOL gDragging = FALSE;
int gDragStartX = 0, gDragStartY = 0;
int gDragScrollX = 0, gDragScrollY = 0;

/* Forward declarations */
void UpdateScrollBars(HWND hwnd);
HICON CreateAppIcon(void);

/* Create a simple programmatic icon (a camera/photograph shape) */
HICON CreateAppIcon(void) {
    int size = 32;
    int cx = GetSystemMetrics(SM_CXICON);
    int cy = GetSystemMetrics(SM_CYICON);
    if (cx > 0 && cy > 0) { size = cx; if (cy < size) size = cy; }

    HDC hdc = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmColor = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hbmMask  = CreateBitmap(size, size, 1, 1, NULL);
    // Fill the mask with white so the icon is fully opaque
    {
        HDC hdcMask = CreateCompatibleDC(hdc);
        HBITMAP hbmOldMask = (HBITMAP)SelectObject(hdcMask, hbmMask);
        PatBlt(hdcMask, 0, 0, size, size, WHITENESS);
        SelectObject(hdcMask, hbmOldMask);
        DeleteDC(hdcMask);
    }
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmColor);

    // Background: dark gray rounded rect
    HBRUSH hbrBg = CreateSolidBrush(RGB(60, 60, 60));
    RECT r = {0, 0, size, size};
    FillRect(hdcMem, &r, hbrBg);
    DeleteObject(hbrBg);

    // White "photo" rectangle
    int margin = size / 8;
    int photoW = size - margin * 2;
    int photoH = size - margin * 2;
    HBRUSH hbrPhoto = CreateSolidBrush(RGB(255, 255, 255));
    RECT rPhoto = {margin, margin, margin + photoW, margin + photoH};
    FillRect(hdcMem, &rPhoto, hbrPhoto);
    DeleteObject(hbrPhoto);

    // Blue "sky" top half
    int skyH = photoH / 3;
    HBRUSH hbrSky = CreateSolidBrush(RGB(100, 160, 240));
    RECT rSky = {margin, margin, margin + photoW, margin + skyH};
    FillRect(hdcMem, &rSky, hbrSky);
    DeleteObject(hbrSky);

    // Green "ground" bottom strip
    int groundH = photoH / 4;
    HBRUSH hbrGround = CreateSolidBrush(RGB(80, 180, 80));
    RECT rGround = {margin, margin + photoH - groundH, margin + photoW, margin + photoH};
    FillRect(hdcMem, &rGround, hbrGround);
    DeleteObject(hbrGround);

    // Yellow "sun" circle
    int sunR = size / 10;
    HBRUSH hbrSun = CreateSolidBrush(RGB(255, 220, 60));
    SelectObject(hdcMem, GetStockObject(NULL_PEN));
    SelectObject(hdcMem, hbrSun);
    Ellipse(hdcMem, margin + photoW - sunR * 3, margin + skyH / 4,
            margin + photoW - sunR,     margin + skyH / 4 + sunR * 2);
    DeleteObject(hbrSun);

    // Dark border around the photo
    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(40, 40, 40));
    SelectObject(hdcMem, hPen);
    SelectObject(hdcMem, GetStockObject(NULL_BRUSH));
    Rectangle(hdcMem, margin, margin, margin + photoW, margin + photoH);
    DeleteObject(hPen);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdc);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = hbmColor;
    ii.hbmMask  = hbmMask;
    HICON hIcon = CreateIconIndirect(&ii);
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    return hIcon;
}

void OpenImageFile(HWND hwnd, const char* forcedPath) {
    if (forcedPath && *forcedPath) {
        // Clean up any previously loaded image
        if (image_data) { stbi_image_free(image_data); image_data = NULL; }
        if (hBitmap)    { DeleteObject(hBitmap);       hBitmap    = NULL; }

        // Strip surrounding quotes and whitespace that Windows may add for paths with spaces
        char cleanPath[MAX_PATH] = {0};
        const char *src = forcedPath;
        // Skip leading spaces and quotes
        while (*src && (*src == ' ' || *src == '"')) src++;
        // Find end of string
        const char *end = src + strlen(src) - 1;
        // Trim trailing spaces and quotes
        while (end > src && (*end == ' ' || *end == '"')) {
            end--;
        }
        size_t len = (size_t)(end - src + 1);
        if (len >= MAX_PATH) len = MAX_PATH - 1;
        memcpy(cleanPath, src, len);
        cleanPath[len] = '\0';

        image_data = stbi_load(cleanPath, &img_width, &img_height, &img_channels, 0);
        if (image_data) {
            hBitmap = CreateBitmapFromSTB(image_data, img_width, img_height, img_channels);
            if (!hBitmap) {
                // Bitmap creation failed — free the raw data so it doesn't leak
                stbi_image_free(image_data);
                image_data = NULL;
                MessageBox(hwnd, "Failed to create bitmap (out of memory?)", "Error", MB_ICONERROR);
                return;
            }
            gZoom = 1.0f;
            gScrollX = 0; gScrollY = 0;
            UpdateScrollBars(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            char errMsg[MAX_PATH + 64];
            snprintf(errMsg, sizeof(errMsg), "Failed to load image:\n%s", cleanPath);
            MessageBox(hwnd, errMsg, "Error", MB_ICONERROR);
        }
        return;
    }

    OPENFILENAME ofn;
    char szFile[MAX_PATH] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.tga;*.psd;*.gif;*.hdr;*.pic;*.pnm;*.ppm;*.pgm\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        if (image_data) { stbi_image_free(image_data); image_data = NULL; }
        if (hBitmap)    { DeleteObject(hBitmap);       hBitmap    = NULL; }

        image_data = stbi_load(szFile, &img_width, &img_height, &img_channels, 0);
        if (image_data) {
            hBitmap = CreateBitmapFromSTB(image_data, img_width, img_height, img_channels);
            if (!hBitmap) {
                stbi_image_free(image_data);
                image_data = NULL;
                MessageBox(hwnd, "Failed to create bitmap (out of memory?)", "Error", MB_ICONERROR);
                return;
            }
            gZoom = 1.0f;
            gScrollX = 0; gScrollY = 0;
            UpdateScrollBars(hwnd);
            InvalidateRect(hwnd, NULL, TRUE); // Trigger repaint
        } else {
            char errMsg[MAX_PATH + 64];
            snprintf(errMsg, sizeof(errMsg), "Failed to load image:\n%s", szFile);
            MessageBox(hwnd, errMsg, "Error", MB_ICONERROR);
        }
    }
}

/* Update scroll bar range and page size based on zoomed image vs visible area */
void UpdateScrollBars(HWND hwnd) {
    if (!hBitmap) {
        // No image loaded — hide scroll bars
        SCROLLINFO si = {0};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_RANGE | SIF_PAGE;
        si.nMin = 0;
        si.nMax = 0;
        si.nPage = 1;
        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        gScrollX = 0;
        gScrollY = 0;
        return;
    }

    RECT rect;
    GetClientRect(hwnd, &rect);
    int viewW = rect.right;
    int viewH = rect.bottom - 40; // button area

    int imgW = (int)(img_width  * gZoom);
    int imgH = (int)(img_height * gZoom);

    // Horizontal scroll bar
    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (imgW > viewW) ? imgW - 1 : 0;
    si.nPage = viewW;
    if (gScrollX > si.nMax - (int)si.nPage + 1) gScrollX = si.nMax - (int)si.nPage + 1;
    if (gScrollX < 0) gScrollX = 0;
    si.nPos = gScrollX;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);

    // Vertical scroll bar — reinitialize si to avoid stale fields
    SCROLLINFO siV = {0};
    siV.cbSize = sizeof(SCROLLINFO);
    siV.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    siV.nMin = 0;
    siV.nMax = (imgH > viewH) ? imgH - 1 : 0;
    siV.nPage = viewH;
    if (gScrollY > siV.nMax - (int)siV.nPage + 1) gScrollY = siV.nMax - (int)siV.nPage + 1;
    if (gScrollY < 0) gScrollY = 0;
    siV.nPos = gScrollY;
    SetScrollInfo(hwnd, SB_VERT, &siV, TRUE);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            CreateWindow("BUTTON", "Open Image", WS_VISIBLE | WS_CHILD, 10, 10, 100, 30, hwnd, (HMENU)1, NULL, NULL);
            CreateWindow("BUTTON", "+", WS_VISIBLE | WS_CHILD, 120, 10, 30, 30, hwnd, (HMENU)2, NULL, NULL);
            CreateWindow("BUTTON", "-", WS_VISIBLE | WS_CHILD, 160, 10, 30, 30, hwnd, (HMENU)3, NULL, NULL);
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1:
                    OpenImageFile(hwnd, NULL);
                    break;
                case 2:
                    gZoom *= 1.1f;
                    if (gZoom > 10.0f) gZoom = 10.0f;
                    UpdateScrollBars(hwnd);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3:
                    gZoom /= 1.1f;
                    if (gZoom < 0.1f) gZoom = 0.1f;
                    UpdateScrollBars(hwnd);
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
            }
            return 0;

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (delta > 0) {
                gZoom *= 1.1f;
                if (gZoom > 10.0f) gZoom = 10.0f;
            } else if (delta < 0) {
                gZoom /= 1.1f;
                if (gZoom < 0.1f) gZoom = 0.1f;
            }
            UpdateScrollBars(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_HSCROLL: {
            SCROLLINFO si = {0};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_HORZ, &si);
            int newPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINELEFT:  newPos -= 20; break;
                case SB_LINERIGHT: newPos += 20; break;
                case SB_PAGELEFT:  newPos -= si.nPage; break;
                case SB_PAGERIGHT: newPos += si.nPage; break;
                case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
            }
            if (newPos < 0) newPos = 0;
            if (newPos > si.nMax - (int)si.nPage + 1) newPos = si.nMax - (int)si.nPage + 1;
            if (newPos != gScrollX) {
                gScrollX = newPos;
                SetScrollPos(hwnd, SB_HORZ, gScrollX, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_VSCROLL: {
            SCROLLINFO si = {0};
            si.cbSize = sizeof(SCROLLINFO);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            int newPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP:    newPos -= 20; break;
                case SB_LINEDOWN:  newPos += 20; break;
                case SB_PAGEUP:    newPos -= si.nPage; break;
                case SB_PAGEDOWN:  newPos += si.nPage; break;
                case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
            }
            if (newPos < 0) newPos = 0;
            if (newPos > si.nMax - (int)si.nPage + 1) newPos = si.nMax - (int)si.nPage + 1;
            if (newPos != gScrollY) {
                gScrollY = newPos;
                SetScrollPos(hwnd, SB_VERT, gScrollY, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            gDragging = TRUE;
            gDragStartX = LOWORD(lParam);
            gDragStartY = HIWORD(lParam);
            gDragScrollX = gScrollX;
            gDragScrollY = gScrollY;
            SetCapture(hwnd);
            return 0;
        }

        case WM_LBUTTONUP: {
            gDragging = FALSE;
            ReleaseCapture();
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (gDragging) {
                int dx = LOWORD(lParam) - gDragStartX;
                int dy = HIWORD(lParam) - gDragStartY;
                gScrollX = gDragScrollX - dx;
                gScrollY = gDragScrollY - dy;

                SCROLLINFO si = {0};
                si.cbSize = sizeof(SCROLLINFO);
                si.fMask = SIF_RANGE | SIF_PAGE;
                GetScrollInfo(hwnd, SB_HORZ, &si);
                int maxX = si.nMax - (int)si.nPage + 1;
                if (gScrollX < 0) gScrollX = 0;
                if (gScrollX > maxX) gScrollX = maxX;

                GetScrollInfo(hwnd, SB_VERT, &si);
                int maxY = si.nMax - (int)si.nPage + 1;
                if (gScrollY < 0) gScrollY = 0;
                if (gScrollY > maxY) gScrollY = maxY;

                SetScrollPos(hwnd, SB_HORZ, gScrollX, TRUE);
                SetScrollPos(hwnd, SB_VERT, gScrollY, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }

        case WM_ERASEBKGND:
            // Prevent Windows from erasing the background separately.
            // We fill it ourselves in WM_PAINT, eliminating flicker.
            return 1;

        case WM_SIZE:
            UpdateScrollBars(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int winWidth  = clientRect.right;
            int winHeight = clientRect.bottom;

            // --- Double-buffer: draw everything off-screen first, then blit once ---
            HDC hdcBuffer = CreateCompatibleDC(hdc);
            HBITMAP hbmBuffer = CreateCompatibleBitmap(hdc, winWidth, winHeight);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

            // 1) Fill background on the off-screen buffer
            HBRUSH hbrBkg = GetSysColorBrush(COLOR_WINDOW);
            FillRect(hdcBuffer, &clientRect, hbrBkg);

            // 2) Draw the image (if any) on the off-screen buffer
            if (hBitmap) {
                SetStretchBltMode(hdcBuffer, HALFTONE);
                HDC hdcMem = CreateCompatibleDC(hdcBuffer);
                HBITMAP hOldBmpMem = (HBITMAP)SelectObject(hdcMem, hBitmap);

                int viewHeight = winHeight - 40; // button area

                int drawWidth  = (int)(img_width  * gZoom);
                int drawHeight = (int)(img_height * gZoom);

                int offsetX = (winWidth  - drawWidth)  / 2;
                int offsetY = 40 + (viewHeight - drawHeight) / 2;

                if (drawWidth  > winWidth)  offsetX = -gScrollX;
                if (drawHeight > viewHeight) offsetY = 40 - gScrollY;

                StretchBlt(hdcBuffer, offsetX, offsetY, drawWidth, drawHeight,
                           hdcMem, 0, 0, img_width, img_height, SRCCOPY);

                // Restore the original bitmap before deleting the DC
                SelectObject(hdcMem, hOldBmpMem);
                DeleteDC(hdcMem);
            }

            // 3) Blit the finished off-screen buffer to the real screen in one shot
            BitBlt(hdc, 0, 0, winWidth, winHeight, hdcBuffer, 0, 0, SRCCOPY);

            // Cleanup
            SelectObject(hdcBuffer, hbmOld);
            DeleteObject(hbmBuffer);
            DeleteDC(hdcBuffer);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            if (image_data) stbi_image_free(image_data);
            if (hBitmap) DeleteObject(hBitmap);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "PhotoViewerClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = CreateAppIcon();

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Portable C Photo Viewer", WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL, 
                                CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    // If a file path was passed in the command line, load it immediately
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        OpenImageFile(hwnd, lpCmdLine);
    }

    ShowWindow(hwnd, nCmdShow);
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
