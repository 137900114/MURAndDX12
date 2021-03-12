#include <Windows.h>//windows 操作系统的api

#include <d3d12.h>//d3d12 api的核心头文件
#include <dxgi1_4.h>//dxgi 系列api的头文件，和屏幕和格式相关的api
#include <d3dcompiler.h>//包含编译shader的api,如果我们不用预编译好的shader文件的话，可以在程序里调用这个api来编译shader

//pragma comment("a.lib")指令可以引导连接器在连接的时候，尝试连接a.lib库
//在这里我们需要3个库文件d3d12.lib,dxgi.lib,d3dcompiler.lib
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

//com组件相关的头文件之前我们说过，这里不再讨论
#include <wrl.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

ComPtr<IDXGIFactory> mDxgiFactory;
ComPtr<ID3D12Device> mD3dDevice;

#include <stdio.h>

//HINSTANCE HWND都是windows API,它们在接下来的d3d12的初始化过程里面会用到
//它们分别表示了应用程序本体和应用程序窗口相关的内存资源
HINSTANCE hinstance;
HWND winHandle;
int width = 800, height = 600;
bool quit = false;

//一个消息处理函数，程序退出，鼠标点击，键盘输入都会被操作系统转换成消息对象(MSG)
//操作系统会把这些对象转译后丢给WinProc消息处理函数处理，这样我们的应用程序就会对我们的输入有反应了
LRESULT CALLBACK WinProc(HWND, UINT, WPARAM, LPARAM);

//d3d12初始化函数
bool D3DInitialize();
//d3d12每帧更新的函数
void D3DUpdate();

int main() {
	//我们需要创建窗口时需要WNDCLASS对象来注册相关信息
	WNDCLASSEX wc;

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

    hinstance = GetModuleHandle(NULL);

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hInstance = hinstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = "WindowClass";
	wc.lpfnWndProc = WinProc;

	//将wc对象注册为名字为wc.lpszClassName的WNDCLASS对象
	RegisterClassEx(&wc);

	//创建窗口
	//这里着重注意第2("WindowClass")，3("A")，5(0),6(0),7(width),8(height)号参数
	//2代表了想要创建的窗口对应的窗口类的名字（这里是之前刚刚注册好的"WindowClass"）
	//3代表了窗口的标题的名字
	//5，6是生成窗口相对屏幕左上角的位置
	//7，8是生成窗口的长和宽
	winHandle = CreateWindowEx(0,
		"WindowClass", "A",
		WS_OVERLAPPEDWINDOW,
		0, 0, width, height,
		NULL, NULL, hinstance, NULL);

	if (winHandle == NULL) {
		printf("fail to initialize window\n");
		return false;
	}

	//在屏幕上显示窗口，并且保持窗口的更新
	ShowWindow(winHandle, SW_SHOWDEFAULT);
	UpdateWindow(winHandle);


	//实际窗口的大小（不算边框的大小）会和我们之前填的大小有出入
	//我们可以通过这个函数获得真实的窗口大小
	RECT rect;
	::GetClientRect(winHandle, &rect);

	height = rect.bottom - rect.top;
	width = rect.right - rect.left;

	//初始化d3d
	if (!D3DInitialize()) {
		printf("fail to initialize d3d\n");
		return false;
	}


	MSG msg;
	//主循环
	while (!quit) {
		//接收到消息，窗口处理消息
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//否则，做渲染工作
		else {
			D3DUpdate();
		}
	}
	return 0;
}

 
LRESULT CALLBACK WinProc(HWND handle, UINT Msg,
	WPARAM wParam, LPARAM lParam) {
	
	switch (Msg) {
		//如果窗口收到的消息是destroy消息（当我们按下窗口右上角的X键）
	case WM_DESTROY:
		PostQuitMessage(0);
		//将窗口退出状态设置为真，我们退出主循环
		quit = true;
		break;
	}

	return DefWindowProc(handle, Msg, wParam, lParam);
}



bool D3DInitialize() {
	//创建一个idxgifactory对象
	CreateDXGIFactory(IID_PPV_ARGS(&mDxgiFactory));

	//我们用来创建device的目标adapter
	ComPtr<IDXGIAdapter> targetAdapter;
	//目标adapter的得分，得分越高性能越好
	size_t targetAdapterScore = 0;
	//当前我们枚举到的adapter的索引值
	size_t adapterIndex = 0;
	//当前我们枚举到的adapter
	ComPtr<IDXGIAdapter> adapter;

	//EnumAdapter通过索引值找到adapter,如果索引值超出系统中含有的adapter的个数，函数会返回DXGI_ERROR_NOT_FOUND
	while (mDxgiFactory->EnumAdapters(adapterIndex++,&adapter) != DXGI_ERROR_NOT_FOUND) {
		//DXGI_ADAPTER_DESC表示了adapter具体的参数信息
		DXGI_ADAPTER_DESC adaDesc;
		//可以通过adapter的GetDesc函数来获得DXGI_ADAPTER_DESC
		adapter->GetDesc(&adaDesc);

		//我们主要关注一个adapter的共享内存大小，独占显内存的大小
		//这两个概念对接下来理解dx12的资源管理非常重要！
		printf("%ls :"
			"\n\t System shared memory: %.2f G"
			"\n\t Device dedicated memory: %.2f G\n", adaDesc.Description ,
			adaDesc.SharedSystemMemory / 1073741824., 
			adaDesc.DedicatedVideoMemory / 1073741824.);

		//我们用一个adapter的独占显存作为评价一个adapter好坏的标准
		if (targetAdapterScore < adaDesc.DedicatedVideoMemory) {
			targetAdapter = adapter;
			targetAdapterScore = adaDesc.DedicatedVideoMemory;
		}
	}
	
	//根据目标adapter创建device
	D3D12CreateDevice(targetAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mD3dDevice));

	return true;
}

void D3DUpdate() {
	//TODO
}