# Obfs-mf

Obfs-mf, short for obfuscation via metafile, is a program written in C containing two groups of functionality. The first group directly correlates to the experiments conducted regarding MFT metafiles, and concerns data-obfuscation via the technique(s) listed in each. This side of obfs-mf is called from the command line with the program parameter “-i” representing the input of data from a specified file to obfuscate in the volume. The second functionality group concerns each of the “-i” options but from a forensic perspective, i.e. that of gathering obfuscated data from a mounted NTFS volume; this side of the program is called with the “-o” parameter representing the output of data to a specified file.

## Compilation

Clone the [repository](https://github.com/BodneyC/mf-obfs.git) and compile with gcc:

```batch
git clone https://github.com/BodneyC/secdel.git`
cd secdel/
gcc obfs-mf.c -o obfs-mf.exe
```

## Usage

Obfs-mf takes three parameters when called: the first of which is the assigned drive letter of an existing volume on the system upon which the program should act; the input (“-i”) or output (“-o”) parameter to define the functionality of this instance; and, the full path of a file which will either act as the data to hide, or the file to which potentially hidden data will be outputted.

The usage of this program demands thorough "full modify" and "special privileges" over the volume to be altered.

**Command format (miss-call output)**

	Command format:

	        ofs-mf.exe <drive letter> <options> <path to file>

	Options:

	        -o      Output file (for retrieving data)
	        -i      Input file (for hiding data)

## Functionality

With either parameter, the program will attempt to open the volume as a handle and, to allow for data-manipulation, lock the volume. The user is then presented with four options regarding metafile alterations including: the writing of data post-VBR in $Boot; the manipulation of the $BadClus:$Bad data stream and the associated bit(s) of $Bitmap:[unnamed]; the writing of data post sector-zero in $UpCase; and, the writing to/read from the slack space of sequential file-records.

The four options are:
- Writing data to $Boot post-VBR (non-bootable volumes only)
- Reserving clusters in $Bitmap, assigning them as "bad" in $BadClus, and writing to them
- Writing data post first sector in $Upcase (despite potential Unicode translation errors)
- Writing the input file data to the end (unused portions) of the end of sequential file-records
