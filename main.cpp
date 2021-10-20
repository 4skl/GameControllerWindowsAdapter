#pragma comment(lib,"User32.lib")
#pragma comment(dll,"C:\\Windows\\System32\\gdi32.dll")
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <Strsafe.h>
#include <Wingdi.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <functional>

bool fullKeyboardOn = true;

bool leftPressed, rightPressed, 
    breakPressed, shiftPressed, 
    altPressed, ctrlPressed, winPressed,
    yPressedOn, bPressedOn, aPressedOn, xPressedOn,
    startPressedOn, backPressedOn,
    lbPressedOn, rbPressedOn,
    keyDid = true;
UINT wDiv, hDiv;
std::vector<std::pair<LONGLONG, DWORD>> savedKeys;
BYTE *svRawInput; UINT svRawInputSz;


LONGLONG getTimestamp(){
    //printf("lis%u\n", sizeof(ULONGLONG));
    LARGE_INTEGER timestamp;
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency); 
    QueryPerformanceCounter(&timestamp);
    timestamp.QuadPart *= 1000;//ms
    timestamp.QuadPart /= Frequency.QuadPart;
    return timestamp.QuadPart;
}

void deleteSavedKeyTo(LONGLONG to){
    int i = 0; 
    while(savedKeys.size() == 0 && i != savedKeys.size()){
        if(savedKeys[i].first > to){
            savedKeys.erase(savedKeys.begin()+i);
        }
         i++;
    }       
}

DWORD getNearestKey(LONGLONG ref){
    LONGLONG mindif = abs(ref-savedKeys[0].first);
    DWORD nearestKey = savedKeys[0].second;
    for(std::vector<std::pair<LONGLONG, DWORD>>::size_type i = 1; i != savedKeys.size(); i++){
        LONGLONG tdif = abs(ref-savedKeys[i].first);
        if(tdif < mindif){
            mindif = tdif;
            nearestKey = savedKeys[i].second;
        }
    }
    return nearestKey;
}

void doKeyPress(WORD wVk){
    INPUT press;
    press.type = INPUT_KEYBOARD;
    press.ki.wVk = wVk;
    SendInput(1, &press, sizeof(INPUT));
}

void doKeyRelease(WORD wVk){
    INPUT release;
    release.type = INPUT_KEYBOARD;
    release.ki.wVk = wVk;
    release.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &release, sizeof(INPUT));
}

void doKeyClick(WORD wVk){
    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.wVk = wVk;
    SendInput(1, &ip, sizeof(INPUT));
    ip.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}

void doKeyUnicode(WORD key){
    INPUT ip;
    ip.type = INPUT_KEYBOARD;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;
    ip.ki.dwFlags = KEYEVENTF_UNICODE;
    ip.ki.wVk = 0;
    ip.ki.wScan = key;
    SendInput(1, &ip, sizeof(INPUT));
    ip.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    SendInput(1, &ip, sizeof(INPUT));
}


inline bool ltPressed(BYTE *rawInput){
    return (rawInput[6]>>2)&1;
}

inline bool rtPressed(BYTE *rawInput){
    return (rawInput[6]>>3)&1;
}

inline bool lbPressed(BYTE *rawInput){
    return rawInput[6]&1;
}

inline bool rbPressed(BYTE *rawInput){
    return (rawInput[6]>>1)&1;
}

inline bool xPressed(BYTE *rawInput){
    return (rawInput[5]>>4)&1;
}

inline bool aPressed(BYTE *rawInput){
    return (rawInput[5]>>5)&1;
}

inline bool bPressed(BYTE *rawInput){
    return (rawInput[5]>>6)&1;
}

inline bool yPressed(BYTE *rawInput){
    return (rawInput[5]>>7)&1;
}

inline bool startPressed(BYTE *rawInput){
     return (rawInput[6]>>5)&1;
}

inline bool backPressed(BYTE *rawInput){
    return (rawInput[6]>>4)&1;
}

inline bool modePressed(BYTE *rawInput){
    return rawInput[7] == 8;
}

inline bool ljPressed(BYTE *rawInput){
    return (rawInput[6]>>6)&1;
}

inline bool rjPressed(BYTE *rawInput){
    return (rawInput[6]>>7)&1;
}

inline BYTE joystickToDirection(BYTE xAngle, BYTE yAngle){ //To ehance
    BYTE xdif = 0x30;
    BYTE ydif = 0x30;
    if(xAngle < 0x80-xdif){
        if(yAngle < 0x7f-ydif){
            return 7;
        }else
        if(yAngle > 0x7f+ydif){
            return 5;
        }else{
            return 6;
        }
    }else
    if(xAngle > 0x80+xdif){
        if(yAngle < 0x7f-ydif){
            return 1;
        }else
        if(yAngle > 0x7f+ydif){
            return 3;
        }else{
            return 2;
        }
    }else{
        if(yAngle < 0x7f-ydif){
            return 0;
        }else
        if(yAngle > 0x7f+ydif){
            return 4;
        }
    }
    return 8;
}

inline BYTE ljDirection(BYTE *rawInput){//Check mode impact
    return joystickToDirection(rawInput[1],rawInput[2]);
}

inline BYTE rjDirection(BYTE *rawInput){
    return joystickToDirection(rawInput[3],rawInput[4]);
}

inline BYTE arrowDirection(BYTE *rawInput){
    return rawInput[5]&0x0f;
}


DWORD * range(DWORD start, DWORD end){
    DWORD * out = new DWORD[end-start];
    for(DWORD i{end}; i<end;i++){
        out[i-start] = i;
    }
    return out;
}

WORD getKeyboardMapping(BYTE *rawInput){
    
    BYTE ldir;
    BYTE adir;
    if (modePressed(rawInput)) {
       ldir = arrowDirection(rawInput); // MODE => LJ
       adir = ljDirection(rawInput); // MODE => ARROW
    } else {
        ldir = ljDirection(rawInput);
        adir = arrowDirection(rawInput);
    }
    BYTE rdir = rjDirection(rawInput);
    printf("%u : %u | %u : %u %u | %u : %u %u\n", ldir, rawInput[5]%0x0f, adir, rawInput[1], rawInput[2], rdir, rawInput[3], rawInput[4]);
    printf("t: %lld \n", getTimestamp());
    bool joystickUsed = !(ldir == 8 && adir == 8 && rdir == 8);
    rbPressedOn = rbPressed(rawInput);
    lbPressedOn = lbPressed(rawInput);
    if(joystickUsed && lbPressedOn){
        // Used for special keys
        // arrows for {([< /
        if(adir != 8 && rdir == 8 && ldir == 8){
            if(shiftPressed){
                return "})]>"[adir/2];
            }else{
                return "{([<"[adir/2];
            }
        }else{
            //printf("Unic %c\n",";:,.+-/*%=\\@!'?\"`'°^_#~"[(ldir/2*8+rdir/2)*(rbPressedOn ? 2 : 1)]);
            //      | 0 | 1 | 2  | 3  | 4 | 5
            return ";:,.+-/*%=\\@!'?\"`'°^_#~"[((ldir/2)*8+(rdir/2))*(rbPressedOn ? 2 : 1)]; //TODO
        }
    }else{ // only one joystick used (others == 8)
        // Used for numbers and letters
        WORD key = 0x41;
        if(rdir != 8){
            key += rdir; // A-H
        }
        if(ldir != 8){
            key += 8 + ldir; // I-P
        }
        if(adir != 8){
            key += 16 + adir; // Q-X
        }
        if(rbPressedOn){
            key += 24; //Y
        }
        if(lbPressedOn){
            key += 25; //Z => can do number, combined with joystick
            printf("lbon %c\n",key);
        }
        if(key == 'Z'){
            printf("%c\n",key);
        }
        if(0x41 <= key && key <= 0x5B){//LETTER
            return key;
        }else{//NUMBERS
            return 0x30+key%10;
        }
    }
}

void doKey(DWORD savedKey){
    if( ( 0x41 <= savedKey && savedKey <= 0x5A ) || ( 0x30 <= savedKey && savedKey <= 0x3A) ){
        printf("keyClick : %c\n",savedKey);
        doKeyClick(savedKey);
    }else{
        printf("keyUnicode : %c\n",savedKey);
        doKeyUnicode(savedKey);
    }
}

void doKeyboardMapping(BYTE *rawInput){
    BYTE ldir = arrowDirection(rawInput); // MODE => LJ
    BYTE adir = ljDirection(rawInput); // MODE => ARROW
    BYTE rdir = rjDirection(rawInput);
    bool joystickUsed = !(ldir == 8 && adir == 8 && rdir == 8);
    DWORD keyUnicode = getKeyboardMapping(rawInput); //! HERE
    if(keyUnicode != 0 && (joystickUsed || rbPressedOn || lbPressedOn)){
        if(rbPressedOn){//Do Now
            doKey(keyUnicode);
        }else{
            savedKeys.push_back(std::pair<LONGLONG, DWORD>(getTimestamp(),keyUnicode));
            keyDid = false;
        }
    }else if(!keyDid && !joystickUsed){
        deleteSavedKeyTo(getTimestamp()-500);
        if(savedKeys.size() > 0){
            doKey(getNearestKey(getTimestamp()-130));
            keyDid = true;
        }
    }
}

void doInput(BYTE * rawInput, UINT rawInputSz){
    if (rawInputSz > 0 && rawInput[0] == 0) {
        svRawInput = new BYTE[rawInputSz];
        memcpy(svRawInput, rawInput, rawInputSz);
        if (modePressed(rawInput)) { //*Mode pressed
            if(!shiftPressed && (ltPressed(rawInput) || rtPressed(rawInput))){
                doKeyPress(VK_LSHIFT);
                shiftPressed = true;
            }else if(shiftPressed){
                doKeyRelease(VK_LSHIFT);
                shiftPressed = false;
            }

            doKeyboardMapping(rawInput);

            if (backPressed(rawInput)) { //*Back Pressed
                if (ltPressed(rawInput)) { //*LT Pressed
                    // Go to line begining
                    doKeyClick(VK_HOME);
                } else {
                    //Do Backspace
                    doKeyClick(VK_BACK);
                }
            }

            if (startPressed(rawInput)) { //*Start pressed
                // Go to line end
                doKeyClick(VK_END);
            }

            if(aPressed(rawInput)){ //*A pressed
                doKeyClick(VK_SPACE);
                printf("Send Space\n");
            }

            if(bPressed(rawInput)){ //*B pressed
                doKeyClick(VK_RETURN);
                printf("Send Backspace\n");
            }
            
        }else{
            bool commandOn = false;
            if(yPressed(rawInput) && !ctrlPressed){ //Ctrl
                commandOn = true;
                doKeyPress(VK_CONTROL);
                ctrlPressed = true;
            }else if(ctrlPressed){
                doKeyRelease(VK_CONTROL);
                ctrlPressed = false;
            }
            if(bPressed(rawInput) && !altPressed){ //Alt
                commandOn = true;
                doKeyPress(VK_MENU);
                altPressed = true;
            }else if(altPressed){
                doKeyRelease(VK_MENU);
                altPressed = false;
            }
            if(xPressed(rawInput) && !winPressed){ //Win
                commandOn = true;
                doKeyPress(VK_LWIN);
                winPressed = true;
            }else if(winPressed){
                doKeyRelease(VK_LWIN);
                winPressed = false;
            }

            if(commandOn){
                doKeyboardMapping(rawInput);
            }else{
                if (rawInput[1] != 0x80 || rawInput[2] != 0x7f || rawInput[3] != 0x80 || rawInput[4] != 0x7f) { //Dead Zone
                    BYTE s1 = 24;
                    BYTE s2 = 8;//Inverse
                    POINT p;
                    if (GetCursorPos(&p)) {
                        SetCursorPos(p.x + (rawInput[1] - 0x80)/s1 + (rawInput[3] - 0x80)/s2, p.y + (rawInput[2]  - 0x7f)/s1 + (rawInput[4] - 0x7f)/s2);
                    }
                }

                if (rbPressed(rawInput) && !rightPressed) { //*RB pressed
                        printf("Right Press\n");
                        INPUT rightPress;
                        rightPress.type = INPUT_MOUSE;
                        rightPress.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                        SendInput(1, &rightPress, sizeof(INPUT));
                        rightPressed = true;
                }else if (rightPressed) {
                    /*POINT p;
                    if (GetCursorPos(&p)) {
                        POINT pCp{p.x,p.y};
                        ClientToScreen(hwnd, &pCp);
                        printf("x : %i y : %i\n", pCp.x, pCp.y);
                        ScreenToClient(hwnd, &p);*/
                        printf("Right Release\n");
                        INPUT rightRelease;
                        rightRelease.type = INPUT_MOUSE;
                        rightRelease.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                        SendInput(1, &rightRelease, sizeof(INPUT));
                        rightPressed = false;
                    //}
                    //SetPixel(GetDC(hwnd), p.x, p.y, RGB(0, 255, 0));
                }

                //SetCursorPos(GetSystemMetrics(SM_CXFULLSCREEN) / 2, GetSystemMetrics(SM_CYFULLSCREEN) / 2);
                
                if (lbPressed(rawInput)  && !leftPressed) { //*LB pressed
                    /*POINT p;
                    if (GetCursorPos(&p)) {*/
                        printf("Left Press\n");
                        INPUT leftPress;
                        leftPress.type = INPUT_MOUSE;
                        leftPress.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                        SendInput(1, &leftPress, sizeof(INPUT));
                        /*?leftPress.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(1, &leftPress, sizeof(INPUT));
                        */
                        leftPressed = true;
                    //}
                }else if (leftPressed) {
                    /*POINT p;
                    if (GetCursorPos(&p)) {
                        ScreenToClient(hwnd, &p);*/
                        printf("Left Release\n");
                        INPUT leftRelease;
                        leftRelease.type = INPUT_MOUSE;
                        leftRelease.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(1, &leftRelease, sizeof(INPUT));
                        leftPressed = false;
                    //}
                }
                
                /* Temp modif lol
                if (xPressed(rawInput)) { //*X pressed
                    doKeyPress(VK_CANCEL);
                    breakPressed = true;
                }else if (breakPressed) {
                    doKeyRelease(VK_CANCEL);
                    breakPressed = false;
                }
                
                if (yPressed(rawInput) && !yPressedOn) { //*Y pressed
                    doKeyPress('A');
                    yPressedOn = true;
                }else if(yPressedOn){
                    doKeyRelease('A');
                    yPressedOn = false;
                }

                if (bPressed(rawInput) && !bPressedOn) { //*B pressed
                    doKeyPress('Z');
                    bPressedOn = true;
                }else if(bPressedOn){
                    doKeyRelease('Z');
                    bPressedOn = false;
                }

                if (aPressed(rawInput) && !aPressedOn) { //*A pressed
                    doKeyPress('E');
                    aPressedOn = true;
                }else if(aPressedOn){
                    doKeyRelease('E');
                    aPressedOn = false;
                }

                if (xPressed(rawInput) && !xPressedOn) { //*X pressed
                    doKeyPress('R');
                    xPressedOn = true;
                }else if(xPressedOn){
                    doKeyRelease('R');
                    xPressedOn = false;
                }*/

                if (arrowDirection(rawInput) == 0) {// Arrow Up
                    doKeyClick(VK_UP);
                }else if (arrowDirection(rawInput) == 2) {// Arrow Right
                    doKeyClick(VK_RIGHT);
                }else if (arrowDirection(rawInput) == 4) {// Arrow Down
                    doKeyClick(VK_DOWN);
                }else if (arrowDirection(rawInput) == 6) {// Arrow Left
                    doKeyClick(VK_LEFT);
                }

                if(backPressed(rawInput) && !backPressedOn){
                    // Select Previous
                    if(lbPressed(rawInput)){//!
                        doKeyPress(VK_NEXT);
                    }else{
                        doKeyPress(VK_PRIOR);
                    }
                    //Temp modif lol
                    //doKeyPress('D');
                    backPressedOn = true;
                }else if(backPressedOn){
                    //doKeyRelease('D');                
                    // Select Previous
                    if(ltPressed(rawInput)){
                        doKeyRelease(VK_NEXT);
                    }else{
                        doKeyRelease(VK_PRIOR);
                    }
                    backPressedOn = false;
                }

                if(startPressed(rawInput) && !startPressedOn){
                    // Select Next
                    doKeyPress(VK_TAB);
                    //Temp modif lol
                    //doKeyPress('F');
                    startPressedOn = true;
                }else if(startPressedOn){
                    doKeyRelease(VK_TAB);
                    //doKeyRelease('F');
                    startPressedOn = false;
                }
            }
        }
    }
}

void repeat_input(unsigned int interval){
    std::thread([interval]() {
        while (true)
        {
            if(svRawInput != NULL) doInput(svRawInput, svRawInputSz);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }).detach();
}

/* usefull
void repeat_function(std::function<void(void)> func, unsigned int interval)
{
    std::thread([func, interval]() {
        while (true)
        {
            func();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }).detach();
}*/

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    switch (uMsg)
    {
    case WM_SIZE:
        {
            int width = LOWORD(lParam);  // Macro to get the low-order word.
            int height = HIWORD(lParam); // Macro to get the high-order word.
            printf("w : %i, h : %i\n", width, height);
        }
        break;
    
    case WM_INPUT:
        {
            UINT dwSize;

            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, 
                            sizeof(RAWINPUTHEADER));
            LPBYTE lpb = new BYTE[dwSize];
            if (lpb == NULL) 
            {
                return 0;
            } 

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, 
                sizeof(RAWINPUTHEADER)) != dwSize )
                    fprintf(stderr, "GetRawInputData does not return correct size !\n");
            RAWINPUT* raw = (RAWINPUT*)lpb;

            TCHAR szTempOutput[300];
            HRESULT hResult;

            /*if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                hResult = StringCchPrintf(szTempOutput, STRSAFE_MAX_CCH, TEXT(" Kbd: make=%04x Flags:%04x Reserved:%04x ExtraInformation:%08x, msg=%04x VK=%04x \n"), 
                    raw->data.keyboard.MakeCode, 
                    raw->data.keyboard.Flags, 
                    raw->data.keyboard.Reserved, 
                    raw->data.keyboard.ExtraInformation, 
                    raw->data.keyboard.Message, 
                    raw->data.keyboard.VKey);
                if (FAILED(hResult))
                {
                // TODO: write error handler
                }
                OutputDebugString(szTempOutput);
            }else if (raw->header.dwType == RIM_TYPEMOUSE) {
                hResult = StringCchPrintf(szTempOutput, STRSAFE_MAX_CCH, TEXT("Mouse: usFlags=%04x ulButtons=%04x usButtonFlags=%04x usButtonData=%04x ulRawButtons=%04x lLastX=%04x lLastY=%04x ulExtraInformation=%04x\r\n"), 
                    raw->data.mouse.usFlags, 
                    raw->data.mouse.ulButtons, 
                    raw->data.mouse.usButtonFlags, 
                    raw->data.mouse.usButtonData, 
                    raw->data.mouse.ulRawButtons, 
                    raw->data.mouse.lLastX, 
                    raw->data.mouse.lLastY, 
                    raw->data.mouse.ulExtraInformation);

                if (FAILED(hResult))
                {
                // TODO: write error handler
                }
                OutputDebugString(szTempOutput);
            }else */if (raw->header.dwType == RIM_TYPEHID){
                svRawInput = raw->data.hid.bRawData;
                svRawInputSz = raw->data.hid.dwCount*raw->data.hid.dwSizeHid;
                TCHAR valOut[60];
                for(DWORD i = 0; i<svRawInputSz; i++){
                    StringCchPrintf(valOut, STRSAFE_MAX_CCH, TEXT("%s%02x"), valOut, svRawInput[i]);
                }
                hResult = StringCchPrintf(szTempOutput, STRSAFE_MAX_CCH, TEXT("HID: dwSizeHid=%08x dwCount=%08x bRawData=%s\n"), 
                    raw->data.hid.dwSizeHid, 
                    raw->data.hid.dwCount, 
                    valOut);
                doInput(svRawInput, svRawInputSz);
                if (FAILED(hResult))
                {
                // TODO: write error handler
                }
                //fprintf(stderr, szTempOutput);
                //printf("a : %i, b : %i, c : %i\n",arrowDirection(rawInput), ljDirection(rawInput), rjDirection(rawInput));
            }

            //printf("%s\n",szTempOutput);

            delete[] lpb; 
            return 0;
        }
        break;
    }
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {

    //Listing devices
    UINT nDevices;
    PRAWINPUTDEVICELIST pRawInputDeviceList;
    if (GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0) { printf("err1");}
    if ((pRawInputDeviceList = (PRAWINPUTDEVICELIST) malloc(sizeof(RAWINPUTDEVICELIST) * nDevices)) == NULL) {printf("err2");}
    if (GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST)) == -1) {printf("err3");}
    // do the job...

    // list devices
    for(UINT i = 0; i < nDevices; i++){
        printf("Dev %i type : %lu\n", i, pRawInputDeviceList[i].dwType);
        char data[250];
        UINT pcbSize;
        GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, RIDI_DEVICENAME, data, &pcbSize); 
        printf("h : %p %i %s\n", pRawInputDeviceList[i].hDevice, pcbSize, data);
    }

    //Creating windows
    LPCSTR CLASS_NAME = "Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
    0,                              // Optional window styles.
    CLASS_NAME,                     // Window class
    "Learn to Program Windows",    // Window text
    WS_OVERLAPPEDWINDOW,            // Window style

    // Size and position
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

    NULL,       // Parent window    
    NULL,       // Menu
    hInstance,  // Instance handle
    NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }
    nCmdShow = 5;
    printf("%i : %s\n", nCmdShow, ShowWindow(hwnd, nCmdShow) ? "true" : "false");

    //Registering devices
    RAWINPUTDEVICE Rid[2];
            
    Rid[0].usUsagePage = 0x01; 
    Rid[0].usUsage = 0x05; 
    Rid[0].dwFlags = RIDEV_INPUTSINK;                 // adds game pad
    Rid[0].hwndTarget = hwnd;

    Rid[1].usUsagePage = 0x01; 
    Rid[1].usUsage = 0x04; 
    Rid[1].dwFlags = RIDEV_INPUTSINK;                 // adds joystick
    Rid[1].hwndTarget = hwnd;
    
    if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE) {
    //registration failed. Call GetLastError for the cause of the error.
        printf("Registration failed\n");
    }
    printf("Registration : %i 1 : %i\n",Rid[0].hwndTarget, Rid[0].hwndTarget);
    
    //repeat_input(50);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0,  0))      
    {
        //printf("%i : %i\n",msg.hwnd, msg.message);
        TranslateMessage(&msg); 
        DispatchMessage(&msg);
    }
    // after the job, free the RAWINPUTDEVICELIST
    free(pRawInputDeviceList);
    return 0;
}