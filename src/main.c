#include <Windows.h>
#include <sound.h>

#ifdef UNICODE
#define WinMain wWinMain
#endif

LRESULT CALLBACK MainWindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam) {
  switch (Msg) {
  case WM_CLOSE:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, Msg, wParam, lParam);
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_ HINSTANCE hPrevInstance, _In_ PTSTR pCmdLine, _In_ int nCmdShow) {
  WNDCLASSEX wc = { sizeof(wc) };
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = MainWindowProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINLOGO));
  wc.hCursor = LoadCursor(hInstance, MAKEINTRESOURCE(IDC_ARROW));
  wc.hbrBackground = GetStockObject(WHITE_BRUSH);
  wc.lpszClassName = TEXT("MainWindowClass");
  if (!RegisterClassEx(&wc)) return MessageBox(NULL, TEXT("Failed to register a window class!"), TEXT("Error"), MB_ICONERROR);

  HWND hWnd = CreateWindow(wc.lpszClassName, TEXT("Waver"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, hInstance, NULL);
  if (!hWnd) return MessageBox(NULL, TEXT("Failed to create a window!"), TEXT("Error"), MB_ICONERROR);
  ShowWindow(hWnd, nCmdShow);

  HSOUNDPLAYER hPlayer = CreateSoundPlayer(22050, 4096, 4);

  LPVOID lpTestSound = NULL;
  HANDLE hTestFile = CreateFile(TEXT("test.wav"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hTestFile) {
    DWORD dwReadFileSize;
    DWORD dwFileSize = GetFileSize(hTestFile, &dwReadFileSize);
    if (lpTestSound = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize)) {
      ReadFile(hTestFile, lpTestSound, dwFileSize, &dwReadFileSize, NULL);
    }
    CloseHandle(hTestFile);
  }
  HSOUND hTestSound = CreateSound(hPlayer, lpTestSound, TRUE, FALSE);

  MSG msg;
  BOOL bStatus;
  while (bStatus = GetMessage(&msg, NULL, 0, 0)) {
    if (bStatus < 0) {
      DestroyWindow(hWnd);
      return MessageBox(NULL, TEXT("Failed to get a message!"), TEXT("Error"), MB_ICONERROR);
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (hTestSound) DeleteSound(hTestSound);
  if (lpTestSound) HeapFree(GetProcessHeap(), 0, lpTestSound);
  if (hPlayer) DeleteSoundPlayer(hPlayer);
  DestroyWindow(hWnd);
  return (int)msg.wParam;
}
