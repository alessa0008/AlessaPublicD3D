#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "winmm.lib")

//dx sdk 
#include "DXSDK\d3dx9.h"
#if defined _M_X64
#pragma comment(lib, "DXSDK/x64/d3dx9.lib") 
#elif defined _M_IX86
#pragma comment(lib, "DXSDK/x86/d3dx9.lib")
#endif

#include "MinHook/include/MinHook.h" //detour
using namespace std;

#pragma warning (disable: 4244) 
#pragma warning (disable: 4996)
#define _CRT_SECURE_NO_DEPRECATE

//==========================================================================================================================

HMODULE Hand;
LPDIRECT3DDEVICE9 pDevice;

UINT Stride;

D3DVIEWPORT9 Viewport; 
float ScreenCX;
float ScreenCY;

LPD3DXFONT Font; 

IDirect3DVertexShader9* vShader;
UINT vSize;

D3DVERTEXBUFFER_DESC vdesc;

bool InitOnce = true;
LPDIRECT3DTEXTURE9 Red, Green, Blue, Yellow;

int countnum = 0;

static BOOL screenshot_taken = FALSE;
//==========================================================================================================================

int wallhack = 1;				
int esp = 1;					
int line = 1;
int aimbot = 1;
int aimkey = 1;
DWORD Daimkey = VK_SHIFT;
int aimsens = 4;				
int aimfov = 5;					
int aimheight = 2;				



char dlldir[320];
char* GetDirFile(char *name)
{
	static char pldir[320];
	strcpy_s(pldir, dlldir);
	strcat_s(pldir, name);
	return pldir;
}

void Log(const char *fmt, ...)
{
	if (!fmt)	return;

	char		text[4096];
	va_list		ap;
	va_start(ap, fmt);
	vsprintf_s(text, fmt, ap);
	va_end(ap);

	ofstream logfile(GetDirFile("log.txt"), ios::app);
	if (logfile.is_open() && text)	logfile << text << endl;
	logfile.close();
}

//==========================================================================================================================

float GetDistance(float Xx, float Yy, float xX, float yY)
{
	return sqrt((yY - Yy) * (yY - Yy) + (xX - Xx) * (xX - Xx));
}

struct WeaponEspInfo_t
{
	float pOutX, pOutY, RealDistance, vSizeod;
	float CrosshairDistance;
};
std::vector<WeaponEspInfo_t>WeaponEspInfo;

void AddWeapons(LPDIRECT3DDEVICE9 Device)
{
	D3DXMATRIX matrix;
	Device->GetVertexShaderConstantF(13, matrix, 4);

	D3DXVECTOR3 pOut, pIn(0, (float)aimheight, 0);
	float distance = pIn.x * matrix._14 + pIn.y * matrix._24 + pIn.z * matrix._34 + matrix._44;
	D3DXVec3TransformCoord(&pOut, &pIn, &matrix);

	pOut.x = Viewport.X + (1.0f + pOut.x) *Viewport.Width / 2.0f;
	pOut.y = Viewport.Y + (1.0f - pOut.y) *Viewport.Height / 2.0f;

	WeaponEspInfo_t pWeaponEspInfo = { static_cast<float>(pOut.x), static_cast<float>(pOut.y), static_cast<float>(distance*0.1f), static_cast<float>(vSize) };
	WeaponEspInfo.push_back(pWeaponEspInfo);
}

//==========================================================================================================================

IDirect3DPixelShader9* oldsShader;
void DrawBox(IDirect3DDevice9 *pDevice, float x, float y, float w, float h, D3DCOLOR Color)
{
	struct Vertex
	{
		float x, y, z, ht;
		DWORD Color;
	}
	V[4] = { { x, y + h, 0.0f, 0.0f, Color },{ x, y, 0.0f, 0.01f, Color },
	{ x + w, y + h, 0.0f, 0.0f, Color },{ x + w, y, 0.0f, 0.0f, Color } };
	pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
	pDevice->GetPixelShader(&oldsShader);

	pDevice->SetTexture(0, Blue);
	pDevice->SetPixelShader(0);

	
	pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

	
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);

	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, V, sizeof(Vertex));

	pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);

	pDevice->SetPixelShader(oldsShader);
}

void DrawP(LPDIRECT3DDEVICE9 Device, int baseX, int baseY, int baseW, int baseH, D3DCOLOR Cor)
{
	D3DRECT BarRect = { baseX, baseY, baseX + baseW, baseY + baseH };
	Device->Clear(1, &BarRect, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, Cor, 0, 0);
}

void DrawCornerBox(LPDIRECT3DDEVICE9 Device, int x, int y, int w, int h, int borderPx, DWORD borderColor)
{
	DrawP(Device, x - (w / 2), (y - h + borderPx), w / 3, borderPx, borderColor); 
	DrawP(Device, x - (w / 2) + w - w / 3, (y - h + borderPx), w / 3, borderPx, borderColor); 
	DrawP(Device, x - (w / 2), (y - h + borderPx), borderPx, w / 3, borderColor); 
	DrawP(Device, x - (w / 2), (y - h + borderPx) + h - w / 3, borderPx, w / 3, borderColor);  
	DrawP(Device, x - (w / 2), y, w / 3, borderPx, borderColor); 
	DrawP(Device, x - (w / 2) + w - w / 3, y, w / 3, borderPx, borderColor); 
	DrawP(Device, (x + w - borderPx) - (w / 2), (y - h + borderPx), borderPx, w / 3, borderColor);
	DrawP(Device, (x + w - borderPx) - (w / 2), (y - h + borderPx) + h - w / 3, borderPx, w / 3, borderColor);
}

HRESULT DrawString(LPD3DXFONT Font, INT X, INT Y, DWORD dColor, CONST PCHAR cString, ...)
{
	HRESULT hRet;

	CHAR buf[512] = { NULL };
	va_list ArgumentList;
	va_start(ArgumentList, cString);
	_vsnprintf_s(buf, sizeof(buf), sizeof(buf) - strlen(buf), cString, ArgumentList);
	va_end(ArgumentList);

	RECT rc[2];
	SetRect(&rc[0], X, Y, X, 0);
	SetRect(&rc[1], X, Y, X + 50, 50);

	hRet = D3D_OK;

	if (SUCCEEDED(hRet))
	{
		Font->DrawTextA(NULL, buf, -1, &rc[0], DT_NOCLIP, 0xFF000000);
		hRet = Font->DrawTextA(NULL, buf, -1, &rc[1], DT_NOCLIP, dColor);
	}

	return hRet;
}

HRESULT DrawCenteredString(LPD3DXFONT Font, INT X, INT Y, DWORD dColor, CONST PCHAR cString, ...)
{
	HRESULT hRet;

	CHAR buf[512] = { NULL };
	va_list ArgumentList;
	va_start(ArgumentList, cString);
	_vsnprintf_s(buf, sizeof(buf), sizeof(buf) - strlen(buf), cString, ArgumentList);
	va_end(ArgumentList);

	RECT rc[2];
	SetRect(&rc[0], X, Y, X, 0);
	SetRect(&rc[1], X, Y, X + 2, 2);

	hRet = D3D_OK;

	if (SUCCEEDED(hRet))
	{
		Font->DrawTextA(NULL, buf, -1, &rc[0], DT_NOCLIP | DT_CENTER, 0xFF000000);
		hRet = Font->DrawTextA(NULL, buf, -1, &rc[1], DT_NOCLIP | DT_CENTER, dColor);
	}

	return hRet;
}

HRESULT GenerateTexture(IDirect3DDevice9 *pDevice, IDirect3DTexture9 **ppD3Dtex, DWORD colour32)
{
	if (FAILED(pDevice->CreateTexture(8, 8, 1, 0, D3DFMT_A4R4G4B4, D3DPOOL_MANAGED, ppD3Dtex, NULL)))
		return E_FAIL;

	WORD colour16 = ((WORD)((colour32 >> 28) & 0xF) << 12)
		| (WORD)(((colour32 >> 20) & 0xF) << 8)
		| (WORD)(((colour32 >> 12) & 0xF) << 4)
		| (WORD)(((colour32 >> 4) & 0xF) << 0);

	D3DLOCKED_RECT d3dlr;
	(*ppD3Dtex)->LockRect(0, &d3dlr, 0, 0);
	WORD *pDst16 = (WORD*)d3dlr.pBits;

	for (int xy = 0; xy < 8 * 8; xy++)
		*pDst16++ = colour16;

	(*ppD3Dtex)->UnlockRect(0);

	return S_OK;
}

class D3DTLVERTEX
{
public:
	FLOAT X, Y, X2, Y2;
	DWORD Color;
};

IDirect3DPixelShader9* oldlShader;
void DrawLine(IDirect3DDevice9* pDevice, float X, float Y, float X2, float Y2, float Width, D3DCOLOR Color, bool AntiAliased)
{
	D3DTLVERTEX qV[2] = {
		{ (float)X , (float)Y, 0.0f, 1.0f, Color },
	{ (float)X2 , (float)Y2 , 0.0f, 1.0f, Color },
	};
	const DWORD D3DFVF_TL = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

	pDevice->SetFVF(D3DFVF_TL);

	pDevice->GetPixelShader(&oldlShader);

	pDevice->SetTexture(0, NULL);
	pDevice->SetPixelShader(0);

	pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, (AntiAliased ? TRUE : FALSE));

	pDevice->DrawPrimitiveUP(D3DPT_LINELIST, 2, qV, sizeof(D3DTLVERTEX));

	pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE);

	pDevice->SetPixelShader(oldlShader);
}

//==========================================================================================================================

void SaveCfg()
{
	ofstream fout;
	fout.open(GetDirFile("alessapublic.ini"), ios::trunc);
	fout << "wallhack " << wallhack << endl;
	fout << "esp " << esp << endl;
	fout << "line " << line << endl;
	fout << "aimbot" << aimbot << endl;
	fout.close();
}

void LoadCfg()
{
	ifstream fin;
	string Word = "";
	fin.open(GetDirFile("alessapublic.ini"), ifstream::in);
	fin >> Word >> wallhack;
	fin >> Word >> esp;
	fin >> Word >> line;
	fin >> Word >> aimbot;
	fin.close();
}

//==========================================================================================================================

int menuselect = 0;
int Current = true;

int PosX = 30;
int PosY = 27;

int ShowMenu = false; 

POINT Pos;


int CheckTab(int x, int y, int w, int h)
{
	if (ShowMenu)
	{
		GetCursorPos(&Pos);
		ScreenToClient(GetForegroundWindow(), &Pos);
		if (Pos.x > x && Pos.x < x + w && Pos.y > y && Pos.y < y + h)
		{
			if (GetAsyncKeyState(VK_LBUTTON) & 1)
			{
				
			}
			return 2;
		}
	}
	return 0;
}

void WriteText(int x, int y, DWORD color, char *text)
{
	RECT rect;
	SetRect(&rect, x, y, x, y);
	Font->DrawText(0, text, -1, &rect, DT_NOCLIP | DT_LEFT, color);
}

void lWriteText(int x, int y, DWORD color, char *text)
{
	RECT rect;
	SetRect(&rect, x, y, x, y);
	Font->DrawText(0, text, -1, &rect, DT_NOCLIP | DT_RIGHT, color);
}

void Category(LPDIRECT3DDEVICE9 pDevice, char *text)
{
	if (ShowMenu)
	{
		int Check = CheckTab(PosX + 44, (PosY + 51) + (Current * 15), 190, 10);
		DWORD ColorText;

		ColorText = D3DCOLOR_ARGB(255, 255, 0, 255);

		if (Check == 2)
			ColorText = D3DCOLOR_ARGB(255, 255, 255, 255);
				
		if (menuselect == Current)
			ColorText = D3DCOLOR_ARGB(255, 255, 255, 255);

		WriteText(PosX + 44, PosY + 50 + (Current * 15) - 1, ColorText, text);
		lWriteText(PosX + 236, PosY + 50 + (Current * 15) - 1, ColorText, "[-]");
		Current++;
	}
}

void AddItem(LPDIRECT3DDEVICE9 pDevice, char *text, int &var, char **opt, int MaxValue)
{
	if (ShowMenu)
	{
		int Check = CheckTab(PosX + 44, (PosY + 51) + (Current * 15), 190, 10);
		DWORD ColorText;

		if (var)
		{
			ColorText = D3DCOLOR_ARGB(255, 0, 255, 0);
		}
		if (var == 0)
		{
			ColorText = D3DCOLOR_ARGB(255, 255, 0, 0);
		}

		if (Check == 1)
		{
			var++;
			if (var > MaxValue)
				var = 0;
		}

		if (Check == 2)
			ColorText = D3DCOLOR_ARGB(255, 255, 255, 255);

		if (menuselect == Current)
		{
			static int lasttick_right = GetTickCount64();
			static int lasttick_left = GetTickCount64();
			if (GetAsyncKeyState(VK_RIGHT) && GetTickCount64() - lasttick_right > 100)
			{
				lasttick_right = GetTickCount64();
				var++;
				if (var > MaxValue)
					var = 0;
			}
			else if (GetAsyncKeyState(VK_LEFT) && GetTickCount64() - lasttick_left > 100)
			{
				lasttick_left = GetTickCount64();
				var--;
				if (var < 0)
					var = MaxValue;
			}
		}

		if (menuselect == Current)
			ColorText = D3DCOLOR_ARGB(255, 255, 255, 255);
		

		WriteText(PosX + 44, PosY + 50 + (Current * 15) - 1, D3DCOLOR_ARGB(255, 50, 50, 50), text);
		WriteText(PosX + 45, PosY + 51 + (Current * 15) - 1, ColorText, text);

		lWriteText(PosX + 236, PosY + 50 + (Current * 15) - 1, D3DCOLOR_ARGB(255, 100, 100, 100), opt[var]);
		lWriteText(PosX + 237, PosY + 51 + (Current * 15) - 1, ColorText, opt[var]);
		Current++;
	}
}

//==========================================================================================================================

char *opt_OnOff[] = { "[OFF]", "[ON]" };
char *opt_aimbot[] = { "[OFF]", "[SHIFT]" };


void DrawMenu(LPDIRECT3DDEVICE9 pDevice)
{
	static int lasttick_insert = GetTickCount64();
	if (GetAsyncKeyState(VK_INSERT) && GetTickCount64() - lasttick_insert > 100)
	{
		lasttick_insert = GetTickCount64();
		ShowMenu = !ShowMenu;
		SaveCfg();
	}
	

	if (ShowMenu)
	{
		static int lasttick_up = GetTickCount64();
		if (GetAsyncKeyState(VK_UP) && GetTickCount64() - lasttick_up > 50)
		{
			lasttick_up = GetTickCount64();
			menuselect--;
		}

		static int lasttick_down = GetTickCount64();
		if (GetAsyncKeyState(VK_DOWN) && GetTickCount64() - lasttick_down > 50)
		{
			lasttick_down = GetTickCount64();
			menuselect++;
		}

		Current = 1;
		AddItem(pDevice, " WallHack", wallhack, opt_OnOff, 1);
		AddItem(pDevice, " ESP BOX", esp, opt_OnOff, 1);
		AddItem(pDevice, " ESP Line", line, opt_OnOff, 1);
		AddItem(pDevice, " Aimbot", aimbot, opt_aimbot, 1);
						

		if (menuselect >= Current)
			menuselect = 1;

		if (menuselect < 1)
			menuselect = 4;
	}
}

//=====================================================================================================================
