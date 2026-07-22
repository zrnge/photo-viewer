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

    HDC hdc = GetDC(NULL);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdc, hBmp);
    
    // Convert image data to 24-bit RGB if necessary
    int rowSize = width * 3;
    int stride = (rowSize + 3) & ~3; // Align to 4-byte boundary
    unsigned char* rgb_data = (unsigned char*)calloc(stride * height, 1);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int src_idx = (y * width + x) * channels;
            int dst_idx = (y * stride) + (x * 3);
            rgb_data[dst_idx] = data[src_idx];     // R
            rgb_data[dst_idx + 1] = data[src_idx + 1]; // G
            rgb_data[dst_idx + 2] = (channels >= 3) ? data[src_idx + 2] : 0; // B
        }
    }

    SetDIBits(hdc, hBmp, 0, height, rgb_data, &bmi, DIB_RGB_COLORS);
    
    SelectObject(hdc, hOldBmp);
    ReleaseDC(NULL, hdc);
    free(rgb_data);
    
    return hBmp;
}

void OpenImageFile(HWND hwnd, const char* forcedPath) {
    if (forcedPath && *forcedPath) {
        // Clean up any previously loaded image
        if (image_data) stbi_image_free(image_data);
        if (hBitmap) DeleteObject(hBitmap);

        // Strip surrounding quotes and whitespace that Windows may add for paths with spaces
        char cleanPath[MAX_PATH] = {0};
        const char *src = forcedPath;
        // Skip leading spaces and quotes
        while (*src && (*src == ' ' || *src == '"')) src++;
        // Find end of string
        const char *end = src + strlen(src) - 1;
        // Trim trailing spaces and quotes
        while (end > src && (*end == ' ' || *end == '"')) {
            // cast away const to modify temporary buffer later
            end--;
        }
        size_t len = (size_t)(end - src + 1);
        if (len >= MAX_PATH) len = MAX_PATH - 1;
        memcpy(cleanPath, src, len);
        cleanPath[len] = '\0';

        image_data = stbi_load(cleanPath, &img_width, &img_height, &img_channels, 0);
        if (image_data) {
            hBitmap = CreateBitmapFromSTB(image_data, img_width, img_height, img_channels);
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            char errMsg[MAX_PATH + 64];
            sprintf(errMsg, "Failed to load image:\n%s", cleanPath);
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
    ofn.lpstrFilter = "Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        if (image_data) stbi_image_free(image_data);
        if (hBitmap) DeleteObject(hBitmap);

        image_data = stbi_load(szFile, &img_width, &img_height, &img_channels, 0);
        if (image_data) {
            hBitmap = CreateBitmapFromSTB(image_data, img_width, img_height, img_channels);
            InvalidateRect(hwnd, NULL, TRUE); // Trigger repaint
        } else {
            MessageBox(hwnd, "Failed to load image!", "Error", MB_ICONERROR);
        }
    }
}

/* Global zoom factor */
float gZoom = 1.0f; // 1.0 = 100%

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
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3:
                    gZoom /= 1.1f;
                    if (gZoom < 0.1f) gZoom = 0.1f;
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
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_SIZE:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Clear background
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HBRUSH hbrBkg = GetSysColorBrush(COLOR_WINDOW);
            FillRect(hdc, &clientRect, hbrBkg);

            if (hBitmap) {
                SetStretchBltMode(hdc, HALFTONE);
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdcMem, hBitmap);

                RECT rect;
                GetClientRect(hwnd, &rect);
                int winWidth = rect.right;
                int winHeight = rect.bottom - 40; // button area

                int baseWidth = (int)(img_width * gZoom);
                int baseHeight = (int)(img_height * gZoom);
                float imgAspect = (float)baseWidth / (float)baseHeight;
                float winAspect = (float)winWidth / (float)winHeight;
                int drawWidth, drawHeight;
                if (winAspect > imgAspect) {
                    drawHeight = (baseHeight < winHeight) ? baseHeight : winHeight;
                    drawWidth = (int)(drawHeight * imgAspect);
                } else {
                    drawWidth = (baseWidth < winWidth) ? baseWidth : winWidth;
                    drawHeight = (int)(drawWidth / imgAspect);
                }

                int offsetX = (winWidth - drawWidth) / 2;
                int offsetY = 40 + (winHeight - drawHeight) / 2;

                StretchBlt(hdc, offsetX, offsetY, drawWidth, drawHeight, hdcMem, 0, 0, img_width, img_height, SRCCOPY);
                DeleteDC(hdcMem);
            }

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
    switch (uMsg) {
        case WM_CREATE:
            // Create Open button
            CreateWindow("BUTTON", "Open Image", WS_VISIBLE | WS_CHILD, 10, 10, 100, 30, hwnd, (HMENU)1, NULL, NULL);
            // Zoom In button
            CreateWindow("BUTTON", "+", WS_VISIBLE | WS_CHILD, 120, 10, 30, 30, hwnd, (HMENU)2, NULL, NULL);
            // Zoom Out button
            CreateWindow("BUTTON", "-", WS_VISIBLE | WS_CHILD, 160, 10, 30, 30, hwnd, (HMENU)3, NULL, NULL);
            return 0;
            CreateWindow("BUTTON", "Open Image", WS_VISIBLE | WS_CHILD, 10, 10, 100, 30, hwnd, (HMENU)1, NULL, NULL);
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1: // Open Image button
                    OpenImageFile(hwnd, NULL);
                    break;
                case 2: // Zoom In button
                    gZoom *= 1.1f;
                    if (gZoom > 10.0f) gZoom = 10.0f;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3: // Zoom Out button
                    gZoom /= 1.1f;
                    if (gZoom < 0.1f) gZoom = 0.1f;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
            }
            return 0;
        case WM_MOUSEWHEEL:
            {
                // GET_WHEEL_DELTA_WPARAM returns a signed value. Positive = wheel forward (zoom in), negative = wheel back (zoom out)
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                if (delta > 0) {
                    gZoom *= 1.1f;
                    if (gZoom > 10.0f) gZoom = 10.0f;
                } else if (delta < 0) {
                    gZoom /= 1.1f;
                    if (gZoom < 0.1f) gZoom = 0.1f;
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
                case 1: // Open Image button
                    OpenImageFile(hwnd, NULL);
                    break;
                case 2: // Zoom In button
                    gZoom *= 1.1f;
                    if (gZoom > 10.0f) gZoom = 10.0f;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
                case 3: // Zoom Out button
                    gZoom /= 1.1f;
                    if (gZoom < 0.1f) gZoom = 0.1f;
                    InvalidateRect(hwnd, NULL, TRUE);
                    break;
            }
            return 0;
            if (LOWORD(wParam) == 1) {
                OpenImageFile(hwnd, NULL);
            }
            return 0;
        case WM_SIZE:
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Clear background
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HBRUSH hbrBkg = GetSysColorBrush(COLOR_WINDOW);
            FillRect(hdc, &clientRect, hbrBkg);

            if (hBitmap) {
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdcMem, hBitmap);
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                
                // Compute drawing size with zoom factor
                int winWidth = rect.right;
                int winHeight = rect.bottom - 40; // button area
                
                // Base size using zoom
                int baseWidth = (int)(img_width * gZoom);
                int baseHeight = (int)(img_height * gZoom);
                
                // Fit within window while preserving aspect ratio
                float imgAspect = (float)baseWidth / (float)baseHeight;
                float winAspect = (float)winWidth / (float)winHeight;
                int drawWidth, drawHeight;
                if (winAspect > imgAspect) {
                    drawHeight = (baseHeight < winHeight) ? baseHeight : winHeight;
                    drawWidth = (int)(drawHeight * imgAspect);
                } else {
                    drawWidth = (baseWidth < winWidth) ? baseWidth : winWidth;
                    drawHeight = (int)(drawWidth / imgAspect);
                }
                
                // Center image
                int offsetX = (winWidth - drawWidth) / 2;
                int offsetY = 40 + (winHeight - drawHeight) / 2;
                
                StretchBlt(hdc, offsetX, offsetY, drawWidth, drawHeight, hdcMem, 0, 0, img_width, img_height, SRCCOPY);
                
                DeleteDC(hdcMem);
            }
            EndPaint(hwnd, &ps);
            return 0;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Clear the background explicitly to remove old image remnants
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HBRUSH hbrBkg = GetSysColorBrush(COLOR_WINDOW);
            FillRect(hdc, &clientRect, hbrBkg);

            if (hBitmap) {
                SetStretchBltMode(hdc, HALFTONE);
                SetBrushOrgEx(hdc, 0, 0, NULL);
                
                HDC hdcMem = CreateCompatibleDC(hdc);
                SelectObject(hdcMem, hBitmap);
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                
                // Calculate fit dimensions maintaining aspect ratio
                int winWidth = rect.right;
                int winHeight = rect.bottom - 40; // Leave space for button area
                
                float imgAspect = (float)img_width / (float)img_height;
                float winAspect = (float)winWidth / (float)winHeight;
                
                int drawWidth, drawHeight;
                if (winAspect > imgAspect) {
                    // Window is wider than image
                    drawHeight = winHeight;
                    drawWidth = (int)(winHeight * imgAspect);
                } else {
                    // Window is taller than image
                    drawWidth = winWidth;
                    drawHeight = (int)(winWidth / imgAspect);
                }
                
                // Center the image
                int offsetX = (winWidth - drawWidth) / 2;
                int offsetY = 40 + (winHeight - drawHeight) / 2;
                
                StretchBlt(hdc, offsetX, offsetY, drawWidth, drawHeight, hdcMem, 0, 0, img_width, img_height, SRCCOPY);
                
                DeleteDC(hdcMem);
            }
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

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Portable C Photo Viewer", WS_OVERLAPPEDWINDOW, 
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
