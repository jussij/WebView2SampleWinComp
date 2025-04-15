// Copyright (C) Microsoft Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pch.h"

#include "App.h"
#include "AppWindow.h"
#include "CheckFailure.h"
#include "CompositionHost.h"
#include <WinUser.h>

static constexpr LPCWSTR s_subFolder = nullptr;
static constexpr WCHAR c_samplePath[] = L"WebView2SamplePage.html";

AppWindow::AppWindow() : m_winComp(std::make_unique<CompositionHost>())
{
    WCHAR szTitle[MAX_LOADSTRING]; // The title bar text
    CHECK_FAILURE(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
    LoadStringW(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

    // Perform application initialization:
    m_mainWindow = CreateWindowW(
        GetWindowClass(), szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        nullptr, nullptr, hInst, nullptr);
    SetWindowLongPtr(m_mainWindow, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(m_mainWindow, nCmdShow);
    UpdateWindow(m_mainWindow);

    m_sampleUri = GetLocalUri(c_samplePath);
    InitializeWebView();
}

// Register the Win32 window class for the app window.
PCWSTR AppWindow::GetWindowClass()
{
    // Only do this once
    static PCWSTR windowClass = [] {
        static WCHAR windowClass[MAX_LOADSTRING];
        LoadStringW(hInst, IDC_WEBVIEW2SAMPLEWINCOMP, windowClass, MAX_LOADSTRING);

        WNDCLASSEXW wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);

        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProcStatic;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = hInst;
        wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_WEBVIEW2SAMPLEWINCOMP));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WEBVIEW2SAMPLEWINCOMP);
        wcex.lpszClassName = windowClass;
        wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

        RegisterClassExW(&wcex);
        return windowClass;
    }();
    return windowClass;
}

LRESULT CALLBACK AppWindow::WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    if (auto app = (AppWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA))
    {
        if (app->HandleWindowMessage(hWnd, message, wParam, lParam))
        {
            return 0;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

bool AppWindow::HandleWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
    {
        m_winComp->OnMouseMessage(message, wParam, lParam);
        return true;
    }

    switch (message)
    {
    case WM_MOVE:
    {
        if (m_controller)
        {
            m_controller->NotifyParentWindowPositionChanged();
        }
        return true;
    }
    case WM_SIZE:
    {
        RECT availableBounds = {0};
        GetClientRect(m_mainWindow, &availableBounds);
        m_winComp->SetBounds(availableBounds);
        return true;
    }
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            return true;
        case IDM_EXIT:
            CloseAppWindow();
            return true;
        }
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return true;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return true;
    }
    return false;
}

// Message handler for about box.
INT_PTR CALLBACK AppWindow::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void AppWindow::InitializeWebView()
{
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        s_subFolder, nullptr, options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                m_webViewEnvironment = environment;
                wil::com_ptr<ICoreWebView2Environment3>
                    webViewEnvironment3 =
                        m_webViewEnvironment.try_query<ICoreWebView2Environment3>();

                if (webViewEnvironment3)
                {
                    CHECK_FAILURE(
                        webViewEnvironment3->CreateCoreWebView2CompositionController(
                            m_mainWindow,
                            Callback<
                                ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                                this, &AppWindow::OnCreateCoreWebView2ControllerCompleted)
                                .Get()));
                }
                //jussij
                CHECK_FAILURE(environment->QueryInterface(IID_PPV_ARGS(&m_webViewEnvironment9)));

                return S_OK;
            })
            .Get());
    assert(SUCCEEDED(hr));
}
//jussij
bool CreateStream(int resourceID, IStream** ppStream)
{
    if (ppStream == 0)
        return false;

    auto lpName = MAKEINTRESOURCE(resourceID);

    HRSRC hrsrc = FindResource(NULL, lpName, RT_RCDATA);

    if (hrsrc == NULL)
    {
        return false;
    }

    DWORD dwResourceSize = SizeofResource(NULL, hrsrc);
    HGLOBAL hglbImage = LoadResource(NULL, hrsrc);

    if (hglbImage == NULL)
    {
        return false;
    }

    LPVOID pvSourceResourceData = LockResource(hglbImage);

    if (pvSourceResourceData == NULL)
    {
        FreeResource(hglbImage);
        return false;
    }

    HGLOBAL hgblResourceData = GlobalAlloc(GMEM_MOVEABLE, dwResourceSize);

    if (hgblResourceData == NULL)
    {
        UnlockResource(hglbImage);
        FreeResource(hglbImage);
        return false;
    }

    LPVOID pvResourceData = GlobalLock(hgblResourceData);

    if (pvResourceData == NULL)
    {
        GlobalFree(hgblResourceData);
        UnlockResource(hglbImage);
        FreeResource(hglbImage);
        return false;
    }

    CopyMemory(pvResourceData, pvSourceResourceData, dwResourceSize);

    GlobalUnlock(hgblResourceData);

    IStream* ipStream = NULL;

    if (FAILED(CreateStreamOnHGlobal(hgblResourceData, TRUE, &ipStream)))
    {
        GlobalFree(hgblResourceData);
        ipStream = NULL;
    }

    *ppStream = ipStream;

    UnlockResource(hglbImage);
    FreeResource(hglbImage);

    return (ipStream != 0) ? true : false;
}

HRESULT AppWindow::OnCreateCoreWebView2ControllerCompleted(
    HRESULT result, ICoreWebView2CompositionController* compositionController)
{
    if (result == S_OK)
    {
        m_compositionController = compositionController;
        CHECK_FAILURE(m_compositionController->QueryInterface(IID_PPV_ARGS(&m_controller)));
        CHECK_FAILURE(m_controller->get_CoreWebView2(&m_webView));

        // jussij
        if (m_controller != nullptr)
        {
          //CHECK_FAILURE(m_pImpl->m_webView->QueryInterface(IID_PPV_ARGS(&m_pImpl->m_webView2)));
          CHECK_FAILURE(m_webView->QueryInterface(IID_PPV_ARGS(&m_webView2_16)));

          m_webView2_16->add_ContextMenuRequested(
            Callback<ICoreWebView2ContextMenuRequestedEventHandler>(
              [this](
                ICoreWebView2 * sender,
                ICoreWebView2ContextMenuRequestedEventArgs * args)
          {
            wil::com_ptr<ICoreWebView2ContextMenuItemCollection> items;
            CHECK_FAILURE(args->get_MenuItems(&items));
            wil::com_ptr<ICoreWebView2ContextMenuTarget> target;
            CHECK_FAILURE(args->get_ContextMenuTarget(&target));
            COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND context_kind;
            CHECK_FAILURE(target->get_Kind(&context_kind));

            if (context_kind == COREWEBVIEW2_CONTEXT_MENU_TARGET_KIND_PAGE)
            {
              wil::com_ptr<IStream> iconStream;
              wil::com_ptr<ICoreWebView2ContextMenuItem> newMenuItem;

              static int counter = 0;

              counter++;
              bool darkMode = counter % 2;

              int resourceId = (darkMode) ? IDI_PDFEXPORT_DM : IDI_PDFEXPORT_LM;

              // the resource to creates an IStream for the menu item icon loaded from a PNG file
              CreateStream(resourceId, &iconStream);

              auto label = (darkMode) ? L"Export to PDF (dark)" : L"Export to PDF (light)";

              CHECK_FAILURE(m_webViewEnvironment9->CreateContextMenuItem(label, iconStream.get(), COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_COMMAND, &newMenuItem));

              CHECK_FAILURE(items->InsertValueAtIndex(5, newMenuItem.get()));
            }

            return S_OK;
          })
          .Get(),
          &m_contextMenuRequestedToken);

        }

        m_controller->put_IsVisible(true);
        m_webView->Navigate(m_sampleUri.c_str());
    }
    else
    {
        ShowFailure(result, L"Failed to create webview");
    }
    m_winComp->Initialize(this);
    return S_OK;
}

void AppWindow::CloseWebView()
{
    if (m_controller)
    {
        m_controller->Close();
        m_controller = nullptr;
        m_webView = nullptr;
    }
    m_webViewEnvironment = nullptr;
}

void AppWindow::CloseAppWindow()
{
    CloseWebView();
    DestroyWindow(m_mainWindow);
}

std::wstring AppWindow::GetLocalUri(std::wstring relativePath)
{
    std::wstring path = GetLocalPath(relativePath, false);

    wil::com_ptr<IUri> uri;
    CHECK_FAILURE(CreateUri(path.c_str(), Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME, 0, &uri));

    wil::unique_bstr uriBstr;
    CHECK_FAILURE(uri->GetAbsoluteUri(&uriBstr));
    return std::wstring(uriBstr.get());
}

std::wstring AppWindow::GetLocalPath(std::wstring relativePath, bool keep_exe_path)
{
    WCHAR rawPath[MAX_PATH];
    GetModuleFileNameW(hInst, rawPath, MAX_PATH);
    std::wstring path(rawPath);
    if (keep_exe_path)
    {
        path.append(relativePath);
    }
    else
    {
        std::size_t index = path.find_last_of(L"\\") + 1;
        path.replace(index, path.length(), relativePath);
    }
    return path;
}
