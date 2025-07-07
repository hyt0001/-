#include <windows.h>
#include <commctrl.h>
#include <string>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

#define IDC_DATEPICKER 1001
#define IDC_COMBOBOX   1002
#define IDC_SEND_BTN   1003

HANDLE hSerial = INVALID_HANDLE_VALUE;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool InitSerialPort(const char* portName) {
    hSerial = CreateFileA(portName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        MessageBoxA(NULL, "打开串口失败", "错误", MB_OK | MB_ICONERROR);
        return false;
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        MessageBoxA(NULL, "获取串口状态失败", "错误", MB_OK | MB_ICONERROR);
        CloseHandle(hSerial);
        return false;
    }

    dcbSerialParams.BaudRate = CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        MessageBoxA(NULL, "设置串口状态失败", "错误", MB_OK | MB_ICONERROR);
        CloseHandle(hSerial);
        return false;
    }

    return true;
}

void SendData(const std::string& data) {
    if (hSerial == INVALID_HANDLE_VALUE) {
        MessageBoxA(NULL, "串口未打开", "错误", MB_OK | MB_ICONERROR);
        return;
    }
    DWORD bytesWritten;
    std::string sendData = data + "\n";
    WriteFile(hSerial, sendData.c_str(), (DWORD)sendData.length(), &bytesWritten, NULL);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icce = { sizeof(INITCOMMONCONTROLSEX), ICC_DATE_CLASSES };
    InitCommonControlsEx(&icce);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DateEntryWndClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        L"货物生产日期录入",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 200,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    if (!InitSerialPort("COM3")) {
        return 0;
    }

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(hSerial);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hDatePicker, hComboBox, hButton;

    switch (msg) {
    case WM_CREATE:
        // 创建日期选择控件
        hDatePicker = CreateWindowEx(0, DATETIMEPICK_CLASS, NULL,
            WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT,
            20, 20, 200, 25,
            hwnd, (HMENU)IDC_DATEPICKER, NULL, NULL);

        // 创建组合框
        hComboBox = CreateWindowEx(0, WC_COMBOBOX, NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            20, 60, 200, 200,
            hwnd, (HMENU)IDC_COMBOBOX, NULL, NULL);

        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"棕色货物 (TYPE1)");
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"浅粉色货物 (TYPE2)");
        SendMessage(hComboBox, CB_SETCURSEL, 0, 0); // 默认选中第一个

        // 创建按钮
        hButton = CreateWindowEx(0, L"BUTTON", L"录入并发送",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 100, 200, 30,
            hwnd, (HMENU)IDC_SEND_BTN, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SEND_BTN) {
            // 读取日期
            SYSTEMTIME st;
            SendMessage(hDatePicker, DTM_GETSYSTEMTIME, 0, (LPARAM)&st);
            if (st.wYear == 0) {
                MessageBox(hwnd, L"请选择有效日期", L"错误", MB_OK | MB_ICONERROR);
                break;
            }

            // 格式化日期 yyyyMMdd
            wchar_t dateStr[16];
            swprintf(dateStr, 16, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);

            // 读取商品类型
            int sel = (int)SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
            if (sel == CB_ERR) {
                MessageBox(hwnd, L"请选择商品类型", L"错误", MB_OK | MB_ICONERROR);
                break;
            }

            // 商品类型编码
            const wchar_t* typeCode = (sel == 0) ? L"TYPE1" : L"TYPE2";

            // 拼接字符串，例如 "TYPE1-20250620"
            wchar_t sendStr[64];
            swprintf(sendStr, 64, L"%s-%s", typeCode, dateStr);

            // 转为多字节
            char sendBuf[64];
            WideCharToMultiByte(CP_ACP, 0, sendStr, -1, sendBuf, sizeof(sendBuf), NULL, NULL);

            // 发送串口
            SendData(std::string(sendBuf));

            MessageBox(hwnd, L"数据已发送", L"成功", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
