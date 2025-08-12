// MariaGfxImport
// By: MottZilla
//
// This program extracts the graphics assets from Dracula X Chronicles to combine with the Maria PS1 overlay from the decomp project.
// The included overlay data lacks the assets required to make it complete. The User *must* provide either the US or EU version of the PSP ISO.
// The overlay was built by the decomp project: https://github.com/Xeeynamo/sotn-decomp
// No source modifications were made, asset data was removed.


#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "error_recalc.h"
#include "MARIA_STRIPPED.bin.h"
#include "PatchInfo_EU.bin.h"
#include "PatchInfo_US.bin.h"

// Globals
char sISOFilePath[4096];
char sISOFilePathOutput[4096];
char MariaBinPath[4096];
char AboutMsg[] = "Select your PSP ISO and click Import.\nIf that succeeds then click to Select your PS1 BIN.\nClick Import again. If MARIA.BIN is not in the same folder\nyou will be prompted to select it. It should be in the folder with your PSP ISO.\n\nThe Integrated Console Rando option should only be used with that randomizer.\n\nMade possible by the decomp project at:\nhttps://github.com/Xeeynamo/sotn-decomp\n";

HWND PrimaryHwnd,tb_FilePath,bt_ISO,bt_ImportGfx,bt_BIN,bt_ImportMariaToPSX,bt_Mode,bt_MariaToWarningTIM,bt_About;

typedef enum { 
	IDC_NONE = 1500,IDC_FILEPATH,IDC_ISO,IDC_IMPORTGFX,IDC_BIN,IDC_IMPORTMARIA,IDC_MODE,IDC_CONSOLERANDO,IDC_ABOUT
} IDCs;

int ISO_Identity = 0;
int Function_Mode = 0;

enum isoids {
	ISO_UNKNOWN,
	ISO_EU,
	ISO_US,
	ISO_PSX_US,
};

// Fix Error Correction for a Sector
void fix_eccedc(FILE *fISO,unsigned int sector_num)
{
	unsigned char sector_buf[2352*1];
	fseek(fISO,sector_num*2352,SEEK_SET);	// Seek to Sector
	fread(&sector_buf[0],2352,1,fISO);		// Read Sector to Buffer
	eccedc_init();
	eccedc_generate(&sector_buf[0]);	
	fseek(fISO,sector_num*2352,SEEK_SET);	// Seek to Sector
	fwrite(&sector_buf[0],2352,1,fISO);		// Write Sector to File
}

// Helper to read 32-bit Unsigned LSBF
unsigned int ReadWord(FILE *fIn)
{
	int a,b,c,d;
	a = fgetc(fIn);
	b = fgetc(fIn);
	c = fgetc(fIn);
	d = fgetc(fIn);
	return (a)|(b<<8)|(c<<16)|(d<<24);
}

// File Picker Helper
void FileSelect(char *StringBuffer,char *FilterString, char *TitleString)
{
	OPENFILENAME ofn;
	ZeroMemory( StringBuffer, strlen( StringBuffer ) );
	ZeroMemory( &ofn,      sizeof( ofn ) );
	ofn.lStructSize  = sizeof( ofn );
	ofn.hwndOwner    = NULL;  // If you have a window to center over, put its HANDLE here
	ofn.lpstrFilter  = FilterString;
	ofn.lpstrFile    = (LPSTR)StringBuffer;
	ofn.nMaxFile     = MAX_PATH;
	ofn.lpstrTitle   = TitleString;
	ofn.Flags        = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;
	GetOpenFileNameA( &ofn );
}

// Select PSX Track 1 BIN File
void SelectBIN()	{	FileSelect(sISOFilePath,"BIN\0*.BIN\0Any File\0*.*\0","Select PSX Symphony of the Night Track 1 BIN file..");	}

// Verification that the file is probably valid.
void VerifyBIN()
{
	char Serial[11];
	FILE *fIn = fopen(sISOFilePath,"rb");
	fseek(fIn,0x9340,SEEK_SET);
	for(int i=0;i<10;i++)	{	Serial[i] = fgetc(fIn);		}	Serial[10] = 0;
	ISO_Identity = ISO_UNKNOWN;
	if(strcmp(Serial,"SLUS_00067") == 0)	{	ISO_Identity = ISO_PSX_US;	}
}

// Select PSP ISO file
void SelectISO()	{	FileSelect(sISOFilePath,"ISO\0*.ISO\0Any File\0*.*\0","Select Dracula X Chronicles PSP ISO file..");	}

// Verification that the file is probably either US or EU version of Dracula X Chronicles for PSP
void VerifyISO()
{
	char Serial[11];
	FILE *fIn = fopen(sISOFilePath,"rb");
	fseek(fIn,0x8373,SEEK_SET);
	for(int i=0;i<10;i++)	{	Serial[i] = fgetc(fIn);		}	Serial[10] = 0;
	ISO_Identity = ISO_UNKNOWN;
	if(strcmp(Serial,"ULES-00841") == 0)	{	ISO_Identity = ISO_EU;	}
	if(strcmp(Serial,"ULUS-10277") == 0)	{	ISO_Identity = ISO_US;	}
}

// Select Maria Overlay BIN for injecting to PS1 BIN file
void SelectMariaBin(){	FileSelect(MariaBinPath,"BIN\0*.BIN\0Any File\0*.*\0","Select MARIA.BIN file..");	}

// GUI Mode Change, hides & shows relevant buttons
void ChangeMode()
{
	Function_Mode ^= 1;
	if(Function_Mode)
	{
		ShowWindow(bt_BIN,SW_SHOW);
		ShowWindow(bt_ImportMariaToPSX,SW_SHOW);
		ShowWindow(bt_MariaToWarningTIM,SW_SHOW);
		ShowWindow(bt_ISO,SW_HIDE);
		ShowWindow(bt_ImportGfx,SW_HIDE);
	}
	else
	{
		ShowWindow(bt_BIN,SW_HIDE);
		ShowWindow(bt_ImportMariaToPSX,SW_HIDE);
		ShowWindow(bt_MariaToWarningTIM,SW_HIDE);
		ShowWindow(bt_ISO,SW_SHOW);
		ShowWindow(bt_ImportGfx,SW_SHOW);
	}
}

// Write the MARIA.BIN data into the PS1 BIN file at RIC.BIN or WARNING.TIM location
void ImportMariaBin()
{
	FILE *fMaria;
	FILE *fPSX;
	int MariaFileSize;
	unsigned char MariaSector[0x800];
	int MariaBytesWritten = 0;
	int SectorsWritten = 0;
	unsigned int LBA;
	
	if(ISO_Identity != ISO_PSX_US)	{	MessageBox(NULL, "You must select BIN for Symphony of the Night SLUS_000.67","Error!",MB_ICONEXCLAMATION|MB_OK);	return;		}
	
	fMaria = fopen("maria.bin","rb");
	if(fMaria == NULL)
	{
		SelectMariaBin();
		fMaria = fopen("maria.bin","rb");
		if(fMaria == NULL)	{	MessageBox(NULL, "MARIA.BIN missing, couldn't open, or invalid!","Error!",MB_ICONEXCLAMATION|MB_OK);	return;		}
	}
	
	fseek(fMaria,0,SEEK_END);	MariaFileSize = ftell(fMaria);	fseek(fMaria,0,SEEK_SET);
	// Basic Sanity Check
	if(MariaFileSize < 0x30000 || MariaFileSize > 0x40000)
	{
		fclose(fMaria);
		MessageBox(NULL, "File selected for MARIA.BIN appears to be invalid or can't be opened!","Error!",MB_ICONEXCLAMATION|MB_OK);
		return;
	}
	
	fPSX = fopen(sISOFilePath,"r+b");
	if(fPSX == NULL)
	{
		MessageBox(NULL, "Couldn't open BIN for writing. Is it open by another process?","Error!",MB_ICONEXCLAMATION|MB_OK);
		fclose(fMaria);
		return;		
	}

	// Writing Data into Sectors	
	if(IsDlgButtonChecked(PrimaryHwnd,IDC_CONSOLERANDO)==BST_CHECKED)
	{
		LBA = 24545;	// Set LBA to Warning.TIM
	}
	else
	{
		LBA = 25814;	// Set LBA to RIC.BIN
	}
	fseek(fPSX, ((LBA * 0x930) + 0x18),SEEK_SET);		// Seek to start of User Data for this File

	while(1)
	{
		fread((unsigned char *)&MariaSector[0],0x800,1,fMaria);	// Read Sector worth of File Data
		
		if(SectorsWritten == 0x66)	// Last Sector Handling
		{
			for(int q = 0x6B0;q<0x800;q++)	{	MariaSector[q] = 0;	}	// Zero out the end of the sector since there wasn't enough data to fill it. Precaution
		}
		
		fwrite((unsigned char *)&MariaSector[0],0x800,1,fPSX);
		SectorsWritten++;
		LBA++;
		fseek(fPSX,0x130,SEEK_CUR);
		if(SectorsWritten >=103)
			break;
	}

	// Zeroing the rest of the file we replaced as it is larger than Maria.BIN
	for(int q = 0x0;q<0x800;q++)	{	MariaSector[q] = 0;	}	// Zero Buffer for filling rest of file sectors
	while(1)	// Fill Sectors with Zero Data
	{
		fwrite((unsigned char *)&MariaSector[0],0x800,1,fPSX);
		SectorsWritten++;
		fseek(fPSX,0x130,SEEK_CUR);
		if(SectorsWritten >=116)
			break;
	}
	
	// Fixing ECCEDC for Sectors
	if(IsDlgButtonChecked(PrimaryHwnd,IDC_CONSOLERANDO)==BST_CHECKED)
	{
		LBA = 24545;	// Set LBA to Warning.TIM
	}
	else
	{
		LBA = 25814;	// Set LBA to RIC.BIN
	}
	SectorsWritten = 0;
	while(1)
	{
		fix_eccedc(fPSX,LBA);
		SectorsWritten++;
		LBA++;
		if(SectorsWritten >=116)
			break;
	}	
	
	fclose(fPSX);fclose(fMaria);
	
	MessageBox(NULL, "MARIA.BIN imported to PSX BIN!","Success!",MB_ICONEXCLAMATION|MB_OK);
}

// Extracts assets from PSP ISO and outputs complete MARIA.BIN using the internal data that is missing the assets.
void ImportGfx()
{
	FILE *fPSP;
	FILE *fPSX;
	
	unsigned int PTR_PSP,PTR_PSX,LENGTH;
	unsigned int *PatchData;
	unsigned char Byte;
	
	if(ISO_Identity == ISO_UNKNOWN || ISO_Identity == ISO_PSX_US)
	{	MessageBox(NULL, "You must select ISO for Dracula X Chronicles PSP. Either US and EU version.","Error!",MB_ICONEXCLAMATION|MB_OK);	return;	}
	
	fPSP = fopen(sISOFilePath,"rb");
	if(fPSP == NULL)	{	MessageBox(NULL, "Can't open PSP ISO File.","Error!",MB_ICONEXCLAMATION|MB_OK);	return;	}
	
	fPSX = fopen("MARIA.BIN","wb");
	if(fPSX == NULL)	{	MessageBox(NULL, "Can't create MARIA.BIN. Is it open by another process?","Error!",MB_ICONEXCLAMATION|MB_OK);	fclose(fPSP);	return;	}
	
	if(ISO_Identity == ISO_EU)
	{
		PatchData = (unsigned int *)&PatchInfo_EU_bin[0];
	}
	if(ISO_Identity == ISO_US)
	{
		PatchData = (unsigned int *)&PatchInfo_US_bin[0];
	}
	
	PatchData += 1;
	
	// Create Stripped File Data First in the Stream
	fwrite((unsigned char *)&MARIA_STRIPPED_BIN[0],sizeof(MARIA_STRIPPED_BIN),1,fPSX);
	
	// Process Chunks
	for(int i=0;i<125;i++)
	{
		//printf("%X, %X, %X\n",PatchData[0],PatchData[1],PatchData[2]);	// Debug
		fseek(fPSP,PatchData[0],SEEK_SET);
		LENGTH = PatchData[1];
		fseek(fPSX,PatchData[2],SEEK_SET);
		
		for(int b=0;b<LENGTH;b++)
		{
			Byte = fgetc(fPSP);
			fputc(Byte,fPSX);
		}
		
		PatchData = PatchData + 3;
		
	}
	
	fclose(fPSP);fPSP = NULL;	fclose(fPSX);fPSX = NULL;
	
	MessageBox(NULL, "MARIA.BIN created!","Success!",MB_ICONEXCLAMATION|MB_OK);
	ChangeMode();
}

// Create windows GUI buttons & widgets
void CreateGUIControls()
{
	HWND hwnd = PrimaryHwnd;

	bt_Mode = CreateWindow("BUTTON",
						"Switch Mode",
						WS_BORDER | WS_CHILD | WS_VISIBLE,
						10, 64, 100, 30,
						hwnd, (HMENU) IDC_MODE, NULL, NULL);	
	
	bt_ISO = CreateWindow("BUTTON",
						"Select Dracula X Chronicles PSP ISO",
						WS_BORDER | WS_CHILD | WS_VISIBLE,
						10, 0, 280, 30,
						hwnd, (HMENU) IDC_ISO, NULL, NULL);	
						
	bt_ImportGfx = CreateWindow("BUTTON",
						"Import GFX and Export PSX Maria BIN",
						WS_BORDER | WS_CHILD | WS_VISIBLE,
						340, 0, 280, 30,
						hwnd, (HMENU) IDC_IMPORTGFX, NULL, NULL);	
	
	tb_FilePath = CreateWindow("EDIT",
							"",
							WS_BORDER | WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
							10, 34, 610, 24,
							hwnd, (HMENU) IDC_FILEPATH, NULL, NULL);
							
	bt_BIN = CreateWindow("BUTTON",
						"Select CV SotN PSX Track 1 BIN",
						WS_BORDER | WS_CHILD,
						10, 0, 280, 30,
						hwnd, (HMENU) IDC_BIN, NULL, NULL);	
						
	bt_ImportMariaToPSX = CreateWindow("BUTTON",
						"Import Maria.BIN to PSX BIN",
						WS_BORDER | WS_CHILD,
						340, 0, 280, 30,
						hwnd, (HMENU) IDC_IMPORTMARIA, NULL, NULL);	
						
	bt_MariaToWarningTIM = CreateWindow("BUTTON",
                            "Import for Integrated Console Rando",
                            WS_CHILD | BS_CHECKBOX | BS_AUTOCHECKBOX | BS_LEFT,
                            350, 64, 270, 30,
                            hwnd, (HMENU) IDC_CONSOLERANDO, NULL, NULL); 
                            
	bt_About = CreateWindow("BUTTON",
						"About / Help",
						WS_BORDER | WS_CHILD | WS_VISIBLE,
						115, 64, 100, 30,
						hwnd, (HMENU) IDC_ABOUT, NULL, NULL);	
}

/* This is where all the input to the window goes to */
LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	int msgid;
	switch(Message) {

	    case WM_CREATE:
	        break;

		case WM_COMMAND:
			if(LOWORD(wParam) == IDC_ISO) {
				SelectISO();
				VerifyISO();
				SetWindowText(tb_FilePath,sISOFilePath);
				SendMessage(tb_FilePath,EM_SETSEL,strlen(sISOFilePath),strlen(sISOFilePath));
			}
			if(LOWORD(wParam) == IDC_BIN) {
				SelectBIN();
				VerifyBIN();
				SetWindowText(tb_FilePath,sISOFilePath);
				SendMessage(tb_FilePath,EM_SETSEL,strlen(sISOFilePath),strlen(sISOFilePath));
			}
			
			if(LOWORD(wParam) == IDC_IMPORTGFX) {
				ImportGfx();
			}
			
			if(LOWORD(wParam) == IDC_IMPORTMARIA) {
				ImportMariaBin();
			}
			
			if(LOWORD(wParam) == IDC_MODE) {
				ChangeMode();
			}
			
			if(LOWORD(wParam) == IDC_ABOUT) {
				MessageBox(NULL, AboutMsg,"MariaGfxImport",MB_ICONEXCLAMATION|MB_OK);
			}

			break;

	    		
		/* Upon destruction, tell the main thread to stop */
		case WM_DESTROY: {
			PostQuitMessage(0);
			break;
		}
		
		/* All other messages (a lot of them) are processed using default procedures */
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

/* The 'main' function of Win32 GUI programs: this is where execution starts */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	WNDCLASSEX wc; /* A properties struct of our window */
	HWND hwnd; /* A 'HANDLE', hence the H, or a pointer to our window */
	MSG msg; /* A temporary location for all messages */

	/* zero out the struct and set the stuff we want to modify */
	memset(&wc,0,sizeof(wc));
	wc.cbSize		 = sizeof(WNDCLASSEX);
	wc.lpfnWndProc	 = WndProc; /* This is where we will send messages to */
	wc.hInstance	 = hInstance;
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	
	/* White, COLOR_WINDOW is just a #define for a system color, try Ctrl+Clicking it */
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszClassName = "WindowClass";
	wc.hIcon		 = LoadIcon(NULL, IDI_APPLICATION); /* Load a standard icon */
	wc.hIconSm		 = LoadIcon(NULL, IDI_APPLICATION); /* use the name "A" to use the project icon */

	if(!RegisterClassEx(&wc)) {
		MessageBox(NULL, "Window Registration Failed!","Error!",MB_ICONEXCLAMATION|MB_OK);
		return 0;
	}

	hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,"WindowClass","Maria Gfx Importer By: MottZilla - Build "__DATE__" - "__TIME__,WS_VISIBLE|WS_SYSMENU|WS_MINIMIZEBOX,
		CW_USEDEFAULT, /* x */
		CW_USEDEFAULT, /* y */
		640, /* width */
		130, /* height */
		NULL,NULL,hInstance,NULL);

	if(hwnd == NULL) {
		MessageBox(NULL, "Window Creation Failed!","Error!",MB_ICONEXCLAMATION|MB_OK);
		return 0;
	}
	
	PrimaryHwnd = hwnd;
	CreateGUIControls();

	/*
		This is the heart of our program where all input is processed and 
		sent to WndProc. Note that GetMessage blocks code flow until it receives something, so
		this loop will not produce unreasonably high CPU usage
	*/
	while(GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
		TranslateMessage(&msg); /* Translate key codes to chars if present */
		DispatchMessage(&msg); /* Send it to WndProc */
	}
	return msg.wParam;
}
