#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <windows.h>
#include <winioctl.h>

#define HI_NIBBLE(b) (((b) >> 4) & 0x0F)
#define LO_NIBBLE(b) ((b) & 0x0F)

//------------------------------------------------------------------------------

int retboot (HANDLE hvol, FILE *pfile, int bps, long int bpc, long long int MFTstart, int choice, int drc);
int retrecord (HANDLE hvol, FILE * pfile, long long int MFTstart);

int altbitmap (HANDLE hvol, long int bpc, long long int MFTstart, long int filesize);
void altboot (HANDLE hvol, long int bpc, FILE *pfile, long long int MFTstart, long int filesize, int bps, int choice);
void altbadclus(HANDLE hvol, long int bpc, FILE *pfile, long long int MFTstart, unsigned long clusno, int noofbitstomark, long totclus, int bps, long int filesize);
void altendofrec (HANDLE hvol, FILE *pfile, long long int MFTstart, long int filesize);

int ATTRfind (unsigned char filerecord[],  int ATTRtype);
int DATAattr(unsigned char filerecord[], int ona, long long int datarunLength[], long long int datarunOffset[], long int bpc);
DWORD writefileatsetpoint(HANDLE hvol, FILE *pfile, long int filesize, int bps);
void hexoutput (unsigned char sector[], int bytecount);
unsigned char * dectohex(unsigned long clusno);
long int hextodec(unsigned char hex[], int size);
static bool yesorno (void);
int getstrlen(unsigned char * input, int size);

//------------------------------------------------------------------------------

int main (int argc, char *argv[])
{
	DWORD bytesread, bytesreturned;
	bool vollocked;
	static bool yorn;
	unsigned char sector[512];
	unsigned char temp[8];
	int methodselect, noofbitstomark, bps, writesuccess;
	long int bpc, filesize;
	long long int MFTstart, offtolastclus;
	long totsec, totclus, totbyte;
	unsigned long clusno;
	FILE *pfile;

	//if (argc != 4) { printf("Command format:\n\n\tWST <drive letter> <options> <path to file>\n\nOptions:\n\n\t-o\tOutput file (for retrieving data)\n\t-i\tInput file (for hiding data)\n"); return 1; }
	argv[1] = "T";
	argv[2] = "-o";

	if (argv[2] == "-o")
	{
		argv[3] = "D:\\Users\\BenJC\\Desktop\\blanktext.bin"; //needs to be automated
	} else { argv[3] = "D:\\Users\\BenJC\\Desktop\\randomdata.txt"; }

	char voltoopen[8] = "\\\\.\\";
	strncat(voltoopen, argv[1], 1);
	strncat(voltoopen, ":", 1); //form voltoopen into "\\\\.\\X:"

	HANDLE hvol = CreateFile(voltoopen, GENERIC_READ | GENERIC_WRITE, 3, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hvol==INVALID_HANDLE_VALUE)
  {
		printf("Couldn't open volume\nPlease assure full ownership of volume and run program as administrator\nError code: %d \n", GetLastError());
		return 1;
  }

	vollocked = DeviceIoControl(hvol, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesreturned, NULL);
	if(vollocked == 0)
	{
		printf("Volume could not be accessed\nError code: %d\n",  GetLastError());
		return 1;
	}

	if(argv[2] == "-i")
	{
		pfile = fopen(argv[3], "rb");
		if(pfile == NULL) { printf("Failed to open %s\n", argv[2]); return 1; }
		fseek(pfile, 0, SEEK_END);
		filesize = ftell(pfile);
		fseek(pfile, 0, SEEK_SET);
	} else if (argv[2] == "-o")
	{
		pfile = fopen(argv[3], "wb+");
		if(pfile == NULL) { printf("Failed to create %s\n", argv[2]); return 1; }
	} else {
		printf("Program error. Error code: %d\n",  GetLastError());
		return 1;
	}
		printf("Program error. Error code: %d\n",  GetLastError());

	ReadFile(hvol, sector, sizeof sector, &bytesread, NULL); //read VBR from volume

	for (int a = 0; a<2; a++)	{	temp[a] = sector[0x0B + a];	}
	bps = hextodec(temp, 2);
	bpc = bps * sector[0x0D]; //Bytes per cluster

	for (int a = 0; a<8; a++)	{	temp[a] = sector[0x30 + a];	}
	MFTstart = hextodec(temp, 8);
	MFTstart = (MFTstart*bpc); //MFT offset (bytes)

	for (int a = 0; a<8; a++)	{	temp[a] = sector[0x28 + a];	}
	totsec = hextodec(temp, 8);
	totclus = totsec / sector[0x0D];
	totbyte = totsec * bps;
	offtolastclus = totbyte + bps - bpc;

	if (argv[2] == "-i")
	{
		noofbitstomark = ceil((float)filesize/bpc);

		printf("Method options\n--------------------------------------------------\n No.\tName\t\tAddition Information\n--------------------------------------------------\n 1.\t$Boot\t\t(Non-bootable volume only)\n 2.\t$BadClus\t(None)\n 3.\t$Upcase\t\t(Potential Unicode errors)\n 4.\tEnd of record\t(Volitile)\n--------------------------------------------------\nChoose by No: ");

		while (scanf(" %d", &methodselect) != 1 && methodselect < 1 || methodselect > 4)
		{
			printf("Incorrect selection (1 - 4)\nSelect again: ");
			scanf(" %d", &methodselect);
			while ( getchar() != '\n' );
		} // get methodselect, no new-line

		switch (methodselect)
		{
			case 1: //$Boot
				printf("Warning: Do not proceed if volume is bootable!\nProceed? (y/n): ");
				yorn = yesorno();
				if (yorn == 1) { altboot(hvol, bpc, pfile, MFTstart, filesize, bps, 1); }
				break;
			case 2: //$BadClus
				clusno = altbitmap(hvol, bpc, MFTstart, filesize);
				altbadclus(hvol, bpc, pfile, MFTstart, clusno, noofbitstomark, totclus, bps, filesize); //Return something?
				break;
			case 3: //$Upcase
				printf("Warning: Potential Unicode translation errors\nProceed? (y/n): ");
				yorn = yesorno();
				if (yorn == 1) { altboot(hvol, bpc, pfile, MFTstart, filesize, bps, 2);}
				break;
			case 4: //File-record
				printf("Warning: Data to be hidden in across multiple volatile filerecords\nProceed? (y/n): ");
				yorn = yesorno();
				if (yorn == 1) { altendofrec(hvol, pfile, MFTstart, filesize); }
				break;
			default:
				printf("\nProgram error. Error code: %i", GetLastError());
				break;
		}
	} else if (argv[2] == "-o")
	{
		printf("Method options\n--------------------------------------------------\n No.\tName\t\tAddition Information\n--------------------------------------------------\n 1.\t$Boot\t\t($Boot post VBR)\n 2.\t$BadClus\t(Output all $Bad clusters)\n 3.\t$Upcase\t\t($UpCase post sector 0)\n 4.\tEnd of record\t(Boundaries required)\n--------------------------------------------------\nChoose by No: ");
		while (scanf(" %d", &methodselect) != 1 && methodselect < 1 || methodselect > 4)
		{
			printf("Incorrect selection (1 - 4)\nSelect again: ");
			scanf(" %d", &methodselect);
			while ( getchar() != '\n' );
		} // get methodselect, no new-line

		switch (methodselect)
		{
			case 1: //$Boot
				writesuccess = retboot(hvol, pfile, bps, bpc, MFTstart, 1, 0);
				break;
			case 2: //$BadClus
				writesuccess = retboot(hvol, pfile, bps, bpc, MFTstart, 3, 1);
				break;
			case 3: //$Upcase
				writesuccess = retboot(hvol, pfile, bps, bpc, MFTstart, 2, 0);
				break;
			case 4: //File-record
				writesuccess = retrecord(hvol, pfile, MFTstart);
				break;
			default:
				printf("\nProgram error. Error code: %d", GetLastError());
				break;
		}
		if (writesuccess) { printf("\nFile not retrieved. Program error: %i", GetLastError()); }
	} else
	{
		printf("Program error. Error code: %d\n",  GetLastError());
		return 1;
	}

	if (fclose(pfile) != 0) { printf("\nFile not closed correctly, error code: %d", GetLastError()); }
	else { printf("\nFile closed correctly"); }
	if(CloseHandle(hvol) == 0) { printf("\nHandle not closed correctly, error code: %d\n", GetLastError()); }
	else { printf("\nHandle closed correctly\n"); }
	printf("\nProgram complete\n");

	return 0;
}

//------------------------------------------------------------------------------

int retrecord (HANDLE hvol, FILE * pfile, long long int MFTstart)
{
	long top32 = 0; //SetFilePointer...
	long lo32 = 0; //As above
	long DATAhi32 = 0; //SetFilePointer...
	long DATAlo32 = 0; //As above
	DWORD filepointercurrent, bytesread, byteswritten;
	int offtoendofrec, remainingbytes, noofrec, a, b;
	unsigned char filerecord[1024] = {0x00};
	unsigned char rectowrite[1024] = {0x00};
	unsigned char ch;
	bool wsuccess;

	printf("\nHow many records to be read?\n");
	scanf(" %d", &noofrec);
	while(getchar() != '\n');

	if (MFTstart > (pow(2, 32)-1)) //More than after 2GB mark
	{
		top32 = MFTstart >> 32; //Shift 32 bits
		lo32 = MFTstart & 0xFFFFFFFF; //Mask top 32
	} else { lo32 = MFTstart; } //Setup lo32 alone

	filepointercurrent = SetFilePointer(hvol, lo32, &top32, FILE_BEGIN); //hvol at $MFT

	for(a = 0; a<noofrec; a++)
	{
		ReadFile(hvol, filerecord, sizeof filerecord, &bytesread, NULL); //Read from handle
		filerecord[0x1FE] = filerecord[0x32];
		filerecord[0x1FF] = filerecord[0x33];
		filerecord[0x3FE] = filerecord[0x34];
		filerecord[0x3FF] = filerecord[0x35]; //Cheap way to do fixup

		offtoendofrec = ATTRfind(filerecord, 0xFF);
		offtoendofrec += 8;
		remainingbytes = 1024 - offtoendofrec;

		for(b=0; b<remainingbytes; b++) { rectowrite[b] = filerecord[offtoendofrec+b]; }

		byteswritten = fwrite(rectowrite, 1, remainingbytes, pfile);
		if(byteswritten = remainingbytes) { printf("\n%i bytes taken from filerecord %i", byteswritten, a); }
		else { printf("\n0 bytes written\nError code: %i", GetLastError()); }

		memset(filerecord, 0, sizeof(filerecord));
		memset(rectowrite, 0, sizeof(rectowrite));
	}

	return 0;
}

//------------------------------------------------------------------------------

int retboot (HANDLE hvol, FILE *pfile, int bps, long int bpc, long long int MFTstart, int choice, int drc)
{
	unsigned char *bootclus;
	DWORD bytesread, byteswritten;
	unsigned char filerecord[1024];
	long long int datarunLength[3]; //Had to be array for DATAattr()
	long long int datarunOffset[3]; //As above
	long top32 = 0; //SetFilePointer...
	long lo32 = 0; //As above
	long DATAhi32 = 0; //SetFilePointer...
	long DATAlo32 = 0; //As above


	if (MFTstart > (pow(2, 32)-1)) //More than after 2GB mark
	{
		top32 = MFTstart >> 32; //Shift 32 bits
		lo32 = MFTstart & 0xFFFFFFFF; //Mask top 32
	} else { lo32 = MFTstart; } //Setup lo32 alone

	DWORD filepointercurrent = SetFilePointer(hvol, lo32, &top32, FILE_BEGIN); //hvol at $MFT

	switch (choice)
	{
		case 1:
			filepointercurrent = SetFilePointer(hvol, 0x1C00, NULL, FILE_CURRENT); //hvol at $Boot
			break;
		case 2:
			filepointercurrent = SetFilePointer(hvol, 0x2800, NULL, FILE_CURRENT); //hvol at $UpCase
			break;
		case 3:
			filepointercurrent = SetFilePointer(hvol, 0x2000, NULL, FILE_CURRENT); //hvol at $BadClus
			break;
		default:
			printf("Program error. Error code: %i", GetLastError);
			return 1;
			break;
	}

	ReadFile(hvol, filerecord, sizeof filerecord, &bytesread, NULL); //read from handle

	filerecord[0x1FE] = filerecord[0x32];
	filerecord[0x1FF] = filerecord[0x33];
	filerecord[0x3FE] = filerecord[0x34];
	filerecord[0x3FF] = filerecord[0x35]; //Cheap way to do fixup

	int DATAoff = ATTRfind(filerecord, 0x80); //Offset of $DATA
	int drmalloc = DATAattr(filerecord, DATAoff, datarunLength, datarunOffset, bpc); //Pop arrays with length and offsets

	if (datarunOffset[drc] > (pow(2, 32)-1)) //More than after 2GB mark
	{
		DATAhi32 = datarunOffset[drc] >> 32; //Shift 32 bits
		DATAlo32 = datarunOffset[drc] & 0xFFFFFFFF; //Mask top 32
	} else { DATAlo32 = datarunOffset[drc]; } //Setup lo32 alone

	SetFilePointer(hvol, DATAlo32, &DATAhi32, FILE_BEGIN); //hvol at $DATA
	if (choice != 3) { SetFilePointer(hvol, bps, NULL, FILE_CURRENT); } //hvol at $Boot post-VBR

	bootclus = (unsigned char *) malloc ((datarunLength[drc])-bps);

	ReadFile(hvol, bootclus, (datarunLength[drc])-bps, &bytesread, NULL); //Read clus from handle
	byteswritten = fwrite(bootclus, 1, bpc-bps, pfile);
	if(byteswritten > 0) { printf("\n%i bytes written to file", byteswritten); }
	else { printf("\n0 bytes written\nWrite error. %i", GetLastError()); return 1; }

	return 0;
}

//------------------------------------------------------------------------------

void altendofrec (HANDLE hvol, FILE *pfile, long long int MFTstart, long int filesize)
{
	int offtoendofrec, remainingbytes;
	int frcount = 0;
	int a = 0;
	long int bytesremaining = filesize;
	long MFThi32, MFTlo32;
	DWORD bytesread, byteswritten;
	unsigned char filerecord[1024];
	unsigned char ch;
	bool wsuccess;

	if (MFTstart > (pow(2, 32)-1))
	{
		MFThi32 = MFTstart >> 32;
		MFTlo32 = MFTstart & 0xFFFFFFFF;
	} else { MFThi32 = 0; MFTlo32 = MFTstart; }

	SetFilePointer(hvol, MFTlo32, &MFThi32, FILE_BEGIN); //hvol at $MFT

	while (bytesremaining != 0)
	{
		ReadFile(hvol, filerecord, sizeof filerecord, &bytesread, NULL); //Read from handle
		SetFilePointer(hvol, -0x200, NULL, FILE_CURRENT); //hvol at $MFT
		offtoendofrec = ATTRfind(filerecord, 0xFF);
		offtoendofrec += 8;
		remainingbytes = 1024 - offtoendofrec;

		for (a=0; a<remainingbytes; a++)
		{
			if (bytesremaining != 0)
			{
				filerecord[offtoendofrec+a] = (ch = fgetc(pfile));
				bytesremaining--;
			}
		}

		wsuccess = WriteFile(hvol, filerecord, 1024, &byteswritten, NULL); //Write to volume, return byteswritten
		if (wsuccess) { printf("%d bytes written in filerecord %i\n", byteswritten, frcount); }
		frcount++;
	}

	printf("\nData written accross %i filerecords\nPlease record this number for data retreival", byteswritten, frcount);
}

//------------------------------------------------------------------------------

void altbadclus (HANDLE hvol, long int bpc, FILE * pfile, long long int MFTstart, unsigned long clusno, int noofbitstomark, long totclus, int bps, long int filesize)
{
	unsigned char filerecord[1024];
	DWORD bytesread;
	long MFThi32, MFTlo32, clusremain, top32, lo32;
	int droffset, newdrlength, slCR, slCT, slLT, a, DATAoff, alignment;
	long int newloa, newlor;
	long long int clusnooffset = clusno * bpc;
	unsigned char newdr[0x30] = {'\0'};
	unsigned char clustemp[8], lentemp[8], clusremaintemp[8], otna[4];
	unsigned char *temp = malloc(8);
	unsigned char *newdrtemp = malloc(0x30);
	DWORD filepointercurrent, byteswritten;

	if (clusnooffset > (pow(2, 32)-1)) //More than after 2GB mark
	{
		top32 = clusnooffset >> 32; //Shift 32 bits
		lo32 = clusnooffset & 0xFFFFFFFF; //Mask top 32
	} else { lo32 = clusnooffset; } //Setup lo32 alone

	if (MFTstart > (pow(2, 32)-1))
	{
		MFThi32 = MFTstart >> 32;
		MFTlo32 = MFTstart & 0xFFFFFFFF;
	} else { MFThi32 = 0; MFTlo32 = MFTstart; }

	SetFilePointer(hvol, (MFTlo32+0x2000), &MFThi32, FILE_BEGIN); //hvol at $BadClus
	ReadFile(hvol, filerecord, sizeof filerecord, &bytesread, NULL); //read from handle
	SetFilePointer(hvol, (MFTlo32+0x2000), &MFThi32, FILE_BEGIN); //hvol at $BadClus

	filerecord[0x1FE] = filerecord[0x32];
	filerecord[0x1FF] = filerecord[0x33];
	filerecord[0x3FE] = filerecord[0x34];
	filerecord[0x3FF] = filerecord[0x35];

	DATAoff = (ATTRfind(filerecord, 0x80))+0x18;

	if(filerecord[DATAoff+4] == 0x18 && filerecord[DATAoff+0x18] == 0x80) {	DATAoff += 0x18; } //checks first $DATA and moves to second if needed
	if (filerecord[DATAoff+0x40] == 0x24 && filerecord[DATAoff+0x42] == 0x42) { droffset = DATAoff+0x48; }
	else { printf("Program Error: %d\n", GetLastError()); } //checks $DATA for $Bad

	clusremain = totclus - clusno - 1;

	memset(temp, 0, sizeof(temp));
	temp = dectohex(clusremain);
	slCR = getstrlen(temp, 8);
	for (a = 0; a<slCR; a++) { clusremaintemp[a] = temp[a]; }

	memset(temp, 0, sizeof(temp));
	temp = dectohex(clusno);
	slCT = getstrlen(temp, 8);
	for (a = 0; a<slCT; a++) { clustemp[a] = temp[a]; }

	memset(temp, 0, sizeof(temp));
	temp = dectohex(noofbitstomark);
	slLT = getstrlen(temp, 8);
	for (a = 0; a<slLT; a++) { lentemp[a] = temp[a]; }

	newdr[0] = 0x00 + slCT; //1 - First sparse
	for (a = 0; a<slCT; a++) { newdr[a+1] = clustemp[a]; } //1 - Length field
	int b = ((slLT << 4) & 0xF0) + slCT;
	newdr[(slCT)+1] = b;  //2 - Second none-sparse
	for (a = 0; a<slLT; a++) { newdr[a+slCT+2] = lentemp[a]; } //2 - Length field
	for (a = 0; a<slCT; a++) { newdr[a+slLT+slCT+2] = clustemp[a]; } //2 - Offset field
	newdr[((slCT)*2) + slLT + 2] = 0x00 + slCR; //3 - Sparse
	for (a = 0; a<slCR; a++) { newdr[((slCT)*2) + slLT + 3 + a] = clusremaintemp[a]; } //3 - Length field

	for(a=0; a<0x30; a++) { newdrtemp[a] = newdr[a]; }
	newdrlength = (getstrlen(newdrtemp, 0x2F))+1;
	alignment = 8 - (newdrlength % 8);

	for(a=0; a<newdrlength; a++) { filerecord[droffset+a] = newdr[a]; }
	for(a=0; a<alignment; a++) { filerecord[droffset+newdrlength+a] = 0x00; }
	newloa = (droffset+newdrlength+alignment)-DATAoff;
	newlor = droffset+newdrlength+alignment+8;
	for(a=0; a<4; a++) { filerecord[droffset+newdrlength+alignment+a] = 0xFF; }

	memset(temp, 0, sizeof(temp));
	temp = dectohex(newloa);
	for (a = 0; a<4; a++) { filerecord[DATAoff+4+a] = temp[a]; } //Length of $DATA

	memset(temp, 0, sizeof(temp));
	temp = dectohex(newlor);
	for (a = 0; a<4; a++) { filerecord[0x18+a] = temp[a]; } //Length of record

	filerecord[0x32] = filerecord[0x1FE];
	filerecord[0x33] = filerecord[0x1FF];
	filerecord[0x34] = filerecord[0x3FE];
	filerecord[0x35] = filerecord[0x3FF];

	filerecord[0x1FE] = filerecord[0x30];
	filerecord[0x1FF] = filerecord[0x31];
	filerecord[0x3FE] = filerecord[0x30];
	filerecord[0x3FF] = filerecord[0x31];

	bool wsuccess = WriteFile(hvol, filerecord, 1024, &byteswritten, NULL); //Write to volume, return byteswritten
	if (wsuccess) { printf("\nWriting failed\nError code: %i", GetLastError()); }

	filepointercurrent = SetFilePointer(hvol, lo32, &top32, FILE_BEGIN); //hvol at cluster to write
	byteswritten = writefileatsetpoint(hvol, pfile, filesize, bps);


}

//------------------------------------------------------------------------------

int altbitmap (HANDLE hvol, long int bpc, long long int MFTstart, long int filesize)
{
	unsigned char filerecord[1024];
	DWORD bytesread;
	long long int datarunLength[5];
	long long int datarunOffset[5];
	long MFThi32, MFTlo32, Bittop32, Bitlo32;
	int noofbitstomark = ceil((float)filesize/bpc);
	int noofbytestomark = ceil((float)noofbitstomark/8);
	DWORD filepointercurrent, rsucc, byteswritten;
	int DATAoff, drmalloc, a, b;
	bool bytesfound;
	bool wsuccess;

	if (MFTstart > (pow(2, 32)-1))
	{
		MFThi32 = MFTstart >> 32;
		MFTlo32 = MFTstart & 0xFFFFFFFF;
	} else { MFThi32 = 0; MFTlo32 = MFTstart; }

	filepointercurrent = SetFilePointer(hvol, (MFTlo32+0x1800), &MFThi32, FILE_BEGIN); //hvol at $Bitmap

	ReadFile(hvol, filerecord, sizeof filerecord, &bytesread, NULL); //read from handle

	filerecord[0x1FE] = filerecord[0x32];
	filerecord[0x1FF] = filerecord[0x33];
	filerecord[0x3FE] = filerecord[0x34];
	filerecord[0x3FF] = filerecord[0x35];

	DATAoff = ATTRfind(filerecord, 0x80);
	drmalloc = DATAattr(filerecord, DATAoff, datarunLength, datarunOffset, bpc);

	unsigned char bitfield[datarunLength[0]];

	if (datarunOffset[0] > (pow(2, 32)-1))
	{
		Bittop32 = datarunOffset[0] >> 32;
		Bitlo32 = datarunOffset[0] & 0xFFFFFFFF;
	} else { Bittop32 = 0; Bitlo32 = datarunOffset[0]; }

	filepointercurrent = SetFilePointer(hvol, Bitlo32, &Bittop32, FILE_BEGIN); //hvol at Bit-field
	rsucc = ReadFile(hvol, bitfield, sizeof bitfield, &bytesread, NULL); //read bitfield

	bytesfound = 0;
	a = 0;
	b = 0;

	while (bytesfound == 0)
	{
		while (bitfield[a] != 0x00)	{ a++;}

		for (b = 0; b<noofbytestomark; b++)
		{
			if(bitfield[a+b] == 0x00)
			{
				bytesfound = 1;
			} else { bytesfound =0; }
		}
	}

	if (noofbitstomark % 8 == 0)
	{
		for(b=0; b<(noofbytestomark-1); b++)
		{
			bitfield[a+b] = 0xFF;
		}
	}

	bitfield[a+b] = pow(2, (noofbitstomark%8))-1;

	filepointercurrent = SetFilePointer(hvol, Bitlo32, &Bittop32, FILE_BEGIN); //hvol at Bit-field
	wsuccess = WriteFile(hvol, bitfield, datarunLength[0], &byteswritten, NULL); //Write to volume, return byteswritten

	return a+b;
}

//------------------------------------------------------------------------------

void altboot (HANDLE hvol, long int bpc, FILE *pfile, long long int MFTstart, long int filesize, int bps, int choice)
{
	unsigned char filerecord[1024];
	DWORD bytesread; //To catch no. of byte written
	DWORD byteswritten; //To catch no. of byte read
	long long int datarunLength[1]; //Had to be array for DATAattr()
	long long int datarunOffset[1]; //As above
	long top32 = 0; //SetFilePointer...
	long lo32 = 0; //As above
	long DATAhi32 = 0; //SetFilePointer...
	long DATAlo32 = 0; //As above

	if (MFTstart > (pow(2, 32)-1)) //More than after 2GB mark
	{
		top32 = MFTstart >> 32; //Shift 32 bits
		lo32 = MFTstart & 0xFFFFFFFF; //Mask top 32
	} else { lo32 = MFTstart; } //Setup lo32 alone

	DWORD filepointercurrent = SetFilePointer(hvol, lo32, &top32, FILE_BEGIN); //hvol at $MFT
	filepointercurrent = SetFilePointer(hvol, (choice == 1 ? 0x1C00: 0x2800), NULL, FILE_CURRENT); //hvol at $Boot/$UpCase
	ReadFile(hvol, filerecord, sizeof filerecord, &bytesread, NULL); //read from handle

	filerecord[0x1FE] = filerecord[0x32];
	filerecord[0x1FF] = filerecord[0x33];
	filerecord[0x3FE] = filerecord[0x34];
	filerecord[0x3FF] = filerecord[0x35]; //Cheap way to do fixup

	int DATAoff = ATTRfind(filerecord, 0x80); //Offset of $DATA
	int drmalloc = DATAattr(filerecord, DATAoff, datarunLength, datarunOffset, bpc); //Pop arrays with length and offsets

	if (datarunLength[0] > filesize) //Is there enough space?
	{
		long long int sectorskip = bps + datarunOffset[0];

		if (sectorskip > (pow(2, 32)-1)) //After 2GB mark
		{
			DATAhi32 = sectorskip >> 32; //Shift 32 bits
			DATAlo32 = sectorskip & 0xFFFFFFFF; //Mask top 32
		} else { DATAlo32 = sectorskip; } //Setup lo32 alone
		int offset = SetFilePointer(hvol, DATAlo32, &DATAhi32, FILE_BEGIN);
		byteswritten = writefileatsetpoint(hvol, pfile, filesize, bps);
		(choice == 1 ? printf("\n%d bytes written post-VBR (sector %i)", byteswritten, offset/512) : printf("\n%d bytes written after first sector of $UpCase (sector %i)", byteswritten, offset/512));
	} else { printf("\nNo modifications made."); }

	printf("\nProgram Completed\n");
}

//------------------------------------------------------------------------------

int getstrlen(unsigned char *input, int size)
{
	int a = 0;
	unsigned char temp[0x30] = {'\0'};

	for (a=0; a<size-1; a++) { temp[a] = input[a]; }

	for(a=size-1; a>=0; a--)
	{
		if ((temp[a] == 0x00) && (temp[a+1] != 0x00))
		{
			temp[a] = 0x01;
		}
	}
	return strlen(temp);
}

//------------------------------------------------------------------------------

DWORD writefileatsetpoint(HANDLE hvol, FILE *pfile, long int filesize, int bps)
{
	int secfilesize = filesize + (bps-(filesize % bps)); //To the nearest sector
	unsigned char filearray[secfilesize]; //Array of pfile to the nearest sector

	fseek(pfile, 0, SEEK_SET); //In case I use it at any point

	char ch;
	DWORD byteswritten; //Number of bytes written to the volume [Return Val.]

	for(int a = 0; a < secfilesize; a++) //Populate filearray[]
	{
		if (a < filesize) //Don't write from memory
		{
			filearray[a] = (ch = fgetc(pfile)); //Didn't like: filearray[a] = fgetc(pfile)
		} else { filearray[a] = 0x00; } //Pad array with zeroes
	}

	bool wsuccess = WriteFile(hvol, filearray, secfilesize, &byteswritten, NULL); //Write to volume, return byteswritten
	if (wsuccess == 1) { printf("\nFile contents written successfully"); } else { printf("\nFile contents not written\nError code: %i", GetLastError()); } //if(write successful){Inform user;}

	return byteswritten;
}

//------------------------------------------------------------------------------

int DATAattr(unsigned char filerecord[], int ona, long long int datarunLength[], long long int datarunOffset[], long int bpc)
{
	unsigned char *datarunarray; //temp storage for datarun
	unsigned char Hona[4]; //Bytes from filerecord[] and datarunarray[]
	int a = 0;
	int b = 0; //Counters
	int c = 0; //Number of data runs [Return Val.]

	for (a=0 ; a<4; a++)	{	Hona[a] = filerecord[ona+4+a]; }
	int tona = hextodec(Hona, 4);	//tona = (to) off to next attr

	Hona[0] = filerecord[ona+0x20]; Hona[1] = filerecord[ona+0x21];
	int otdr = hextodec(Hona, 2);
	int lodr = tona - otdr; // lodr = length of data run(s), otdr = offset to data run(s)

	datarunarray = (unsigned char *) malloc (lodr); //Allocate length of DR including alignment

	for (a=0; a<lodr; a++) { datarunarray[a] = filerecord[ona + otdr + a]; } //Populate datarunarray[]
	a=0;

	do {
		int drO = HI_NIBBLE(datarunarray[a]); //Shift and mask header
		int drL = LO_NIBBLE(datarunarray[a]); //Mask header

		for (b = 0; b<drL; b++)	{ Hona[b] = datarunarray[b+a+1]; } //Populate Hona[] with length
		datarunLength[c] = (hextodec(Hona, drL) * bpc); //Needs to be bytes per cluster

		for (b = 0; b<drO; b++)	{ Hona[b] = datarunarray[b+a+drL+1]; } //Populate Hona[] with offset
		datarunOffset[c] = ((datarunarray[drO + drL] > 0x7F ? ((~(hextodec(Hona, drO)))+1) : (hextodec(Hona, drO))) * bpc) + (c>0 ? datarunOffset[c-1] *bpc : 0); //if c>0, append starting cluster to previous

		a += drO + drL + 1; //To next data run
		c++;
	} while (datarunarray[a] != 0x00); //do... while() to catch the first

	free(datarunarray); //Deallocate memory for datarunarray[]

	return c;
}

//------------------------------------------------------------------------------

int ATTRfind (unsigned char filerecord[],  int ATTRtype)
{
	unsigned char Hona[4]; //Bytes from filerecord[]

	Hona[0] = filerecord[0x14]; Hona[1] = filerecord[0x15];
	int ona = hextodec(Hona, 2); //[] to int

	while (filerecord[ona] != ATTRtype)
	{
		for (int a = 0; a<4; a++)	{	Hona[a] = filerecord[ona+4+a]; }
		ona += hextodec(Hona, 4); //ona will = $DATA start
	}

	return ona;
}

//------------------------------------------------------------------------------

long int hextodec(unsigned char hex[], int size)
{
	long int x = 0; //Return value
	int a = 0; //hex[] count
	int b = 0; //Order count

	for (a=0; a < size; a++)
	{
		x = x + (hex[a] * (pow(16, b))); //Multiply each byte by 16^(b+2)
		b=b+2;
	}
	return x;
}

//------------------------------------------------------------------------------

void hexoutput (unsigned char sector[], int bytecount)
{
	int i = 0; //Overall byte count
	int k = 0; //Line count
	int j = 0; //Sector line count
	int a = 0; //Char count
	unsigned char ch;

	printf("Hexadecimal Output\n");
	printf("\n        0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f  : 0 1 2 3 4 5 6 7 8 9 a b c d e f\n");
	printf("       ------------------------------------------------ : -------------------------------\n0x%03x | ", j);

	j+=16;
	while( i != bytecount )
	{
		if (k != 15) //Print one char, repeat
		{
			printf("%02x ", sector[i]);
			k++;
		} else if (k == 15)
		{
			if(j % 512 != 0) //Check for sector break
			{
				printf("%02x : ", sector[i]);
				i-=15; //Return line for char
				for (a=0; a <= 15; a++) //char loop
				{
					if((sector[i] <= 0x20) || (sector[i] == 0x7F))
					{
						printf(". ");
					} else {
						printf("%c ", sector[i]);
					}
					i++; //Compensate for subbing 15
				}
				i--;
				printf("\n0x%03x | ", j); //New line
				k = 0; //For the new line
				j = j+16; //Setup next line
			} else {
				printf("%02x : ", sector[i]); //Last byte of hex
				i-=15;
				for (a=0; a <= 15; a++) //Print chars
				{
					if((sector[i] <= 0x20) || (sector[i] == 0x7F)) //ASCII control
					{
						printf(". ");
					} else {
						printf("%c ", sector[i]);
					}
					i++;
				}
				i--;
				if(i != bytecount-1)
				{
					printf("\n       ------------------------------------------------ : -------------------------------");
					printf("\n0x%03x | ", j);
				}
				j=j+16;
				k = 0;
			}
		}
	i++;
	}
}

//------------------------------------------------------------------------------

unsigned char * dectohex(unsigned long clusno)
{
	static unsigned char temp[4];
	unsigned int tclusno = clusno;

	memset(temp, 0, sizeof(temp));

	for(int a = 0; a < 5; a++)
	{
		temp[a] = tclusno & 0xff; //mask a byte
		tclusno = tclusno >> 8; //shift a byte
	}
	temp[strlen(temp)] = '\0'; //terminate

	return temp;
}

//------------------------------------------------------------------------------

static bool yesorno (void)
{
	static char yorn;
	if (scanf(" %c", &yorn) != 1 && yorn != 'y' || yorn != 'n') //Not sure why it works but it doesn't work without it
	{
		while (yorn != 'y' && yorn != 'n' && yorn != 'Y' && yorn != 'Y') //case insensitivity
		{
			printf("Incorrect input ('y' or 'n'): ");
			scanf(" %c", &yorn);
			while(getchar() != '\n'); //catch return
		}
	} // get yorn, no new-line
	if (yorn =='y' || yorn == 'Y') { return 1; } else { return 0; } //set bool
}
