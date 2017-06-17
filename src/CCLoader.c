
/*

Copyright (c) 2012-2014 RedBearLab

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
#define _BSD_SOURCE

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>

#include "CCLoader.h"

#define HIGH '1'
#define LOW '0'
#define INPUT "in"
#define OUTPUT "out"

int cachedFiles[32] = { 0 };

struct args args = {
	.DC = 2,
	.DD = 3,
	.RESET = 4,
	.retries = 5,
	.verify = 1,
	.fName = NULL
};

void delay(int ms)
{
	usleep(ms);
	return;
}

int unexportPin(int pin)
{
	char buf[16];
	const char path[] = "/sys/class/gpio/unexport";
	FILE *pFile = fopen(path, "w");
	if (!pFile) {
		char err[64];
		sprintf(err, "Error opening file %s", path);
		perror(err);
		return -1;
	}

	sprintf(buf, "%d", pin);
	if (!fwrite(buf, strlen(buf), 1, pFile)) {
		char err[64];
		sprintf(err, "Error writing file %s", path);
		perror(err);
		return -1;
	}
	fwrite("\n", 1, 1, pFile);

	fclose(pFile);

	return 0;
}

int exportPin(int pin)
{
	char buf[16];
	const char path[] = "/sys/class/gpio/export";
	FILE *pFile = fopen(path, "w");
	if (!pFile) {
		char err[64];
		sprintf(err, "Error opening file %s", path);
		perror(err);
		return -1;
	}

	sprintf(buf, "%d", pin);
	if (!fwrite(buf, strlen(buf), 1, pFile)) {
		char err[64];
		sprintf(err, "Error writing file %s", path);
		perror(err);
		return -1;
	}
	fwrite("\n", 1, 1, pFile);

	fclose(pFile);

	return 0;
}

int digitalWrite(int pin, const char state)
{
	if(cachedFiles[pin] == 0) {
		char path[64];
		sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
		cachedFiles[pin] = open(path, O_RDWR);

		if (cachedFiles[pin] == -1) {
			char err[64];
			sprintf(err, "Error opening file %s", path);
			perror(err);
			return -1;
		}
	}

	if (!write(cachedFiles[pin], &state, 1)) {
		char err[64];
		sprintf(err, "Error writing to pin %d", pin);
		perror(err);
		return -1;
	}

	return 0;
}

const char digitalRead(int pin)
{
	if(cachedFiles[pin] == 0) {
		char path[64];
		sprintf(path, "/sys/class/gpio/gpio%d/value", pin);
		cachedFiles[pin] = open(path, O_RDWR);

		if (cachedFiles[pin] == -1) {
			char err[64];
			sprintf(err, "Error opening file %s", path);
			perror(err);
			return -1;
		}
	}
	else
		lseek(cachedFiles[pin], 0, SEEK_SET);

	char output;

	if (read(cachedFiles[pin], &output, 1) != 1) {
		char err[64];
		sprintf(err, "Error reading pin %d", pin);
		perror(err);
		return -1;
	}

	return output;
}

int pinMode(int pin, const char *mode)
{
	char path[64];
	FILE *file;

	sprintf(path, "/sys/class/gpio/gpio%d/direction", pin);
	file = fopen(path, "w");
	if (!file) {
		char err[64];
		sprintf(err, "Error opening file %s", path);
		perror(err);
		return -1;
	}

	if (!fwrite(mode, strlen(mode), 1, file)) {
		char err[64];
		sprintf(err, "Error writing file %s", path);
		perror(err);
		return -1;
	}
	fwrite("\n", 1, 1, file);

	fclose(file);

	return 0;
}

/**************************************************************************//**
* @brief    Writes a byte on the debug interface. Requires DD to be
*           output when function is called.
* @param    data    Byte to write
* @return   None.
******************************************************************************/
void write_debug_byte(unsigned char data)
{
	unsigned char i;
	for (i = 0; i < 8; i++) {
		// Set clock high and put data on DD line
		digitalWrite(args.DC, HIGH);
		if (data & 0x80) {
			digitalWrite(args.DD, HIGH);
		} else {
			digitalWrite(args.DD, LOW);
		}
		data <<= 1;
		digitalWrite(args.DC, LOW);	// set clock low (DUP capture flank)
	}
}

/**************************************************************************//**
* @brief    Reads a byte from the debug interface. Requires DD to be
*           input when function is called.
* @return   Returns the byte read.
******************************************************************************/
unsigned char read_debug_byte(void)
{
	unsigned char i;
	unsigned char data = 0x00;
	for (i = 0; i < 8; i++) {
		digitalWrite(args.DC, HIGH);	// DC high
		data <<= 1;
		if (HIGH == digitalRead(args.DD)) {
			data |= 0x01;
		}
		digitalWrite(args.DC, LOW);	// DC low
	}
	return data;
}

/**************************************************************************//**
* @brief    Function waits for DUP to indicate that it is ready. The DUP will
*           pulls DD line low when it is ready. Requires DD to be input when
*           function is called.
* @return   Returns 0 if function timed out waiting for DD line to go low
* @return   Returns 1 when DUP has indicated it is ready.
******************************************************************************/
unsigned char wait_dup_ready(void)
{
	// DUP pulls DD low when ready
	unsigned int count = 0;
	while ((HIGH == digitalRead(args.DD)) && count < 16) {
		// Clock out 8 bits before checking if DD is low again
		read_debug_byte();
		count++;
	}
	return (count == 16) ? 0 : 1;
}

/**************************************************************************//**
* @brief    Issues a command on the debug interface. Only commands that return
*           one output byte are supported.
* @param    cmd             Command byte
* @param    cmd_bytes       Pointer to the array of data bytes following the
*                           command byte [0-3]
* @param    num_cmd_bytes   The number of data bytes (input to DUP) [0-3]
* @return   Data returned by command
******************************************************************************/
unsigned char
debug_command(unsigned char cmd, unsigned char *cmd_bytes,
	      unsigned short num_cmd_bytes)
{
	unsigned short i;
	unsigned char output = 0;
	// Make sure DD is output
	pinMode(args.DD, OUTPUT);
	// Send command
	write_debug_byte(cmd);
	// Send bytes
	for (i = 0; i < num_cmd_bytes; i++) {
		write_debug_byte(cmd_bytes[i]);
	}
	// Set DD as input
	pinMode(args.DD, INPUT);
	digitalWrite(args.DD, HIGH);
	// Wait for data to be ready
	wait_dup_ready();
	// Read returned byte
	output = read_debug_byte();
	// Set DD as output
	pinMode(args.DD, OUTPUT);

	return output;
}

/**************************************************************************//**
* @brief    Resets the DUP into debug mode. Function assumes that
*           the programmer I/O has already been configured using e.g.
*           ProgrammerInit().
* @return   None.
******************************************************************************/
void debug_init(void)
{
	// Send two flanks on DC while keeping RESET_N low
	// All low (incl. RESET_N)
	digitalWrite(args.DD, LOW);
	digitalWrite(args.DC, LOW);
	digitalWrite(args.RESET, LOW);
	delay(10);		// Wait
	digitalWrite(args.DC, HIGH);	// DC high
	delay(10);		// Wait
	digitalWrite(args.DC, LOW);	// DC low
	delay(10);		// Wait
	digitalWrite(args.DC, HIGH);	// DC high
	delay(10);		// Wait
	digitalWrite(args.DC, LOW);	// DC low
	delay(10);		// Wait
	digitalWrite(args.RESET, HIGH);	// Release RESET_N
	delay(10);		// Wait
}

/**************************************************************************//**
* @brief    Reads the chip ID over the debug interface using the
*           GET_CHIP_ID command.
* @return   Returns the chip id returned by the DUP
******************************************************************************/
unsigned char read_chip_id(void)
{
	unsigned char id = 0;

	// Make sure DD is output
	pinMode(args.DD, OUTPUT);
	delay(1);
	// Send command
	write_debug_byte(CMD_GET_CHIP_ID);
	// Set DD as input
	pinMode(args.DD, INPUT);
	digitalWrite(args.DD, HIGH);
	delay(1);
	// Wait for data to be ready
	if (wait_dup_ready() == 1) {
		// Read ID and revision
		id = read_debug_byte();	// ID
		read_debug_byte();	// Revision (discard)
	}
	// Set DD as output
	pinMode(args.DD, OUTPUT);

	return id;
}

/**************************************************************************//**
* @brief    Sends a block of data over the debug interface using the
*           BURST_WRITE command.
* @param    src         Pointer to the array of input bytes
* @param    num_bytes   The number of input bytes
* @return   None.
******************************************************************************/
void burst_write_block(unsigned char *src, unsigned short num_bytes)
{
	unsigned short i;

	// Make sure DD is output
	pinMode(args.DD, OUTPUT);

	write_debug_byte(CMD_BURST_WRITE | HIBYTE(num_bytes));
	write_debug_byte(LOBYTE(num_bytes));
	for (i = 0; i < num_bytes; i++) {
		write_debug_byte(src[i]);
	}

	// Set DD as input
	pinMode(args.DD, INPUT);
	digitalWrite(args.DD, HIGH);
	// Wait for DUP to be ready
	wait_dup_ready();
	read_debug_byte();	// ignore output
	// Set DD as output
	pinMode(args.DD, OUTPUT);
}

/**************************************************************************//**
* @brief    Issues a CHIP_ERASE command on the debug interface and waits for it
*           to complete.
* @return   None.
******************************************************************************/
void chip_erase(void)
{
	volatile unsigned char status;
	// Send command
	debug_command(CMD_CHIP_ERASE, 0, 0);

	// Wait for status bit 7 to go low
	do {
		status = debug_command(CMD_READ_STATUS, 0, 0);
	}
	while ((status & STATUS_CHIP_ERASE_BUSY_BM));
}

/**************************************************************************//**
* @brief    Writes a block of data to the DUP's XDATA space.
* @param    address     XDATA start address
* @param    values      Pointer to the array of bytes to write
* @param    num_bytes   Number of bytes to write
* @return   None.
******************************************************************************/
void
write_xdata_memory_block(unsigned short address,
			 const unsigned char *values, unsigned short num_bytes)
{
	unsigned char instr[3];
	unsigned short i;

	// MOV DPTR, address
	instr[0] = 0x90;
	instr[1] = HIBYTE(address);
	instr[2] = LOBYTE(address);
	debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

	for (i = 0; i < num_bytes; i++) {
		// MOV A, values[i]
		instr[0] = 0x74;
		instr[1] = values[i];
		debug_command(CMD_DEBUG_INSTR_2B, instr, 2);

		// MOV @DPTR, A
		instr[0] = 0xF0;
		debug_command(CMD_DEBUG_INSTR_1B, instr, 1);

		// INC DPTR
		instr[0] = 0xA3;
		debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
	}
}

/**************************************************************************//**
* @brief    Writes a byte to a specific address in the DUP's XDATA space.
* @param    address     XDATA address
* @param    value       Value to write
* @return   None.
******************************************************************************/
void write_xdata_memory(unsigned short address, unsigned char value)
{
	unsigned char instr[3];

	// MOV DPTR, address
	instr[0] = 0x90;
	instr[1] = HIBYTE(address);
	instr[2] = LOBYTE(address);
	debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

	// MOV A, values[i]
	instr[0] = 0x74;
	instr[1] = value;
	debug_command(CMD_DEBUG_INSTR_2B, instr, 2);

	// MOV @DPTR, A
	instr[0] = 0xF0;
	debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
}

/**************************************************************************//**
* @brief    Read a byte from a specific address in the DUP's XDATA space.
* @param    address     XDATA address
* @return   Value read from XDATA
******************************************************************************/
unsigned char read_xdata_memory(unsigned short address)
{
	unsigned char instr[3];

	// MOV DPTR, address
	instr[0] = 0x90;
	instr[1] = HIBYTE(address);
	instr[2] = LOBYTE(address);
	debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

	// MOVX A, @DPTR
	instr[0] = 0xE0;
	return debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
}

/**************************************************************************//**
* @brief    Reads 1-32767 bytes from DUP's flash to a given buffer on the
*           programmer.
* @param    bank        Flash bank to read from [0-7]
* @param    address     Flash memory start address [0x0000 - 0x7FFF]
* @param    values      Pointer to destination buffer.
* @return   None.
******************************************************************************/
void
read_flash_memory_block(unsigned char bank, unsigned short flash_addr,
			unsigned short num_bytes, unsigned char *values)
{
	unsigned char instr[3];
	unsigned short i;
	unsigned short xdata_addr = (0x8000 + flash_addr);

	// 1. Map flash memory bank to XDATA address 0x8000-0xFFFF
	write_xdata_memory(DUP_MEMCTR, bank);

	// 2. Move data pointer to XDATA address (MOV DPTR, xdata_addr)
	instr[0] = 0x90;
	instr[1] = HIBYTE(xdata_addr);
	instr[2] = LOBYTE(xdata_addr);
	debug_command(CMD_DEBUG_INSTR_3B, instr, 3);

	for (i = 0; i < num_bytes; i++) {
		// 3. Move value pointed to by DPTR to accumulator (MOVX A, @DPTR)
		instr[0] = 0xE0;
		values[i] = debug_command(CMD_DEBUG_INSTR_1B, instr, 1);

		// 4. Increment data pointer (INC DPTR)
		instr[0] = 0xA3;
		debug_command(CMD_DEBUG_INSTR_1B, instr, 1);
	}
}

/**************************************************************************//**
* @brief    Writes 4-2048 bytes to DUP's flash memory. Parameter \c num_bytes
*           must be a multiple of 4.
* @param    src         Pointer to programmer's source buffer (in XDATA space)
* @param    start_addr  FLASH memory start address [0x0000 - 0x7FFF]
* @param    num_bytes   Number of bytes to transfer [4-1024]
* @return   None.
******************************************************************************/
void
write_flash_memory_block(unsigned char *src, unsigned long start_addr,
			 unsigned short num_bytes)
{
	// 1. Write the 2 DMA descriptors to RAM
	write_xdata_memory_block(ADDR_DMA_DESC_0, dma_desc_0, 8);
	write_xdata_memory_block(ADDR_DMA_DESC_1, dma_desc_1, 8);

	// 2. Update LEN value in DUP's DMA descriptors
	unsigned char len[2] = { HIBYTE(num_bytes), LOBYTE(num_bytes) };
	write_xdata_memory_block((ADDR_DMA_DESC_0 + 4), len, 2);	// LEN, DBG => ram
	write_xdata_memory_block((ADDR_DMA_DESC_1 + 4), len, 2);	// LEN, ram => flash

	// 3. Set DMA controller pointer to the DMA descriptors
	write_xdata_memory(DUP_DMA0CFGH, HIBYTE(ADDR_DMA_DESC_0));
	write_xdata_memory(DUP_DMA0CFGL, LOBYTE(ADDR_DMA_DESC_0));
	write_xdata_memory(DUP_DMA1CFGH, HIBYTE(ADDR_DMA_DESC_1));
	write_xdata_memory(DUP_DMA1CFGL, LOBYTE(ADDR_DMA_DESC_1));

	// 4. Set Flash controller start address (wants 16MSb of 18 bit address)
	write_xdata_memory(DUP_FADDRH, HIBYTE((start_addr)));	//>>2) ));
	write_xdata_memory(DUP_FADDRL, LOBYTE((start_addr)));	//>>2) ));

	// 5. Arm DBG=>buffer DMA channel and start burst write
	write_xdata_memory(DUP_DMAARM, CH_DBG_TO_BUF0);
	burst_write_block(src, num_bytes);

	// 6. Start programming: buffer to flash
	write_xdata_memory(DUP_DMAARM, CH_BUF0_TO_FLASH);
	write_xdata_memory(DUP_FCTL, 0x0A);	//0x06

	// 7. Wait until flash controller is done
	while (read_xdata_memory(DUP_FCTL) & 0x80) ;
}

void RunDUP(void)
{
	// Send two flanks on DC while keeping RESET_N low
	// All low (incl. RESET_N)
	digitalWrite(args.DD, LOW);
	digitalWrite(args.DC, LOW);
	digitalWrite(args.RESET, LOW);
	delay(10);		// Wait

	digitalWrite(args.RESET, HIGH);
	delay(10);		// Wait
}

int ProgrammerInit(void)
{
	if (pinMode(args.DD, OUTPUT))
		return -1;
	if (pinMode(args.DC, OUTPUT))
		return -1;
	if (pinMode(args.RESET, OUTPUT))
		return -1;
	if (digitalWrite(args.DD, LOW))
		return -1;
	if (digitalWrite(args.DC, LOW))
		return -1;
	if (digitalWrite(args.RESET, HIGH))
		return -1;

	return 0;
}

int
flash_block(FILE * pFile, unsigned char *rxBuf, unsigned char Verify,
	    unsigned int addr, int retries)
{
	write_flash_memory_block(rxBuf, addr, 512);	// src, address, count                    
	if (Verify) {
		unsigned char bank = addr / (512 * 16);
		unsigned int offset = (addr % (512 * 16)) * 4;
		unsigned char read_data[512];
		read_flash_memory_block(bank, offset, 512, read_data);	// Bank, address, count, dest.            
		for (unsigned int i = 0; i < 512; i++) {
			if (read_data[i] != rxBuf[i]) {
				fprintf(stderr,
					"\nError verifying byte %d of block %d, trying again.\n",
					i, addr / 128);
				if (retries)
					return flash_block(pFile, rxBuf, Verify,
							   addr, retries - 1);
				else
					return -1;
			}
		}
	}

	return 0;
}

void flash_chip(FILE * pFile, long fSize, unsigned char Verify)
{
	printf("Flashing firmware...");
	fflush(stdout);

	// Program data (start address must be word aligned [32 bit])
	unsigned char rxBuf[514];
	unsigned int addr = 0;
	for (int i = 0; i < fSize / 512; i++) {
		if (fread(rxBuf, 512, 1, pFile) == 0) {
			perror("Error reading from file");
			return;
		}

		printf("\rFlashing firmware... (%d blocks written, %d%% done)",
		       i, 100 * i * 512 / fSize);
		fflush(stdout);

		if (flash_block(pFile, rxBuf, Verify, addr, 10))
			return;

		addr += (unsigned int)128;
	}

	// add 0xFF to the end of the last block if neccessary
	if (fSize % 512) {
		if (fread(rxBuf, fSize % 512, 1, pFile) == 0) {
			perror("Error reading from file");
			return;
		}

		memset(rxBuf + fSize % 512, 0xff, 512 - (fSize % 512));

		printf
		    ("\rFlashing firmware... (%d blocks written, 100%% done)\t",
		     fSize / 512);
		fflush(stdout);

		int retries = 3;
		while (--retries && flash_block(pFile, rxBuf, Verify, addr, 10))
			fprintf(stderr,
				"\nError verifying block %d, trying again.\n",
				fSize / 512 + 1);
		if (!retries)
			return;

	}
	printf("\nFlashing has completed successfully.\n");

}

error_t arg_parser(int key, char *arg, struct argp_state * state)
{
	char *endptr;
	int val;
	error_t err = 0;

	switch (key) {
	case 'v':
		args.verify = 0;
		printf("Verify disabled\n");
		break;
	case 'r':
		val = strtol(arg, &endptr, 10);
		if (arg == endptr)
			errno = EINVAL;
		if (errno == EINVAL || errno == ERANGE) {
			err = errno;
			break;
		}

		if (val < 0 || val > 15) {
			printf("Number of retries must be between 0-15.\n");
			err = EINVAL;
			break;
		}

		args.retries = val;
		printf("Retries: %d\n", args.retries);
		return 0;
	case 'C':
		val = strtol(arg, &endptr, 10);
		if (arg == endptr)
			errno = EINVAL;
		if (errno == EINVAL || errno == ERANGE) {
			err = errno;
			break;
		}

		args.DC = val;
		printf("DC: pin %d\n", args.DC);
		return 0;
	case 'D':
		val = strtol(arg, &endptr, 10);
		if (arg == endptr)
			errno = EINVAL;
		if (errno == EINVAL || errno == ERANGE) {
			err = errno;
			break;
		}

		args.DD = val;
		printf("DD: pin %d\n", args.DD);
		return 0;
	case 'R':
		val = strtol(arg, &endptr, 10);
		if (arg == endptr)
			errno = EINVAL;
		if (errno == EINVAL || errno == ERANGE) {
			err = errno;
			break;
		}

		args.RESET = val;
		printf("RESET: pin %d\n", args.RESET);
		return 0;
	case ARGP_KEY_ARG:
		if (args.fName) {
			err = EINVAL;
			break;
		}
		args.fName = arg;
		break;
	case ARGP_KEY_END:
		if (args.fName == NULL)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	if (err)
		argp_usage(state);
	else
		return 0;
}

int main(int argc, char **argv)
{
	unsigned char chip_id;
	unsigned char debug_config = 0x22;
	unsigned char Verify = 1;
	long fSize;
	FILE *pFile;
	struct argp_option options[] = {
		{
		 .name = "no-verify",
		 .key = 'v',
		 .doc = "Do not verify each block after it is flashed"},
		{
		 .name = "retries",
		 .key = 'r',
		 .arg = "RETRIES",
		 .doc =
		 "Number of time to retry flashing a block after it fails verification (defaults to 5)"},
		{
		 .name = "DC",
		 .key = 'C',
		 .arg = "pin",
		 .doc =
		 "Raspberry Pi pin connected to the DEBUG_CLOCK (DC) pin on the CC254x chip"},
		{
		 .name = "DD",
		 .key = 'D',
		 .arg = "pin",
		 .doc =
		 "Raspberry Pi pin connected to the DEBUG_DATA (DD) pin on the CC254x chip"},
		{
		 .name = "RESET",
		 .key = 'R',
		 .arg = "pin",
		 .doc =
		 "Raspberry Pi pin connected to the RESET_N pin on the CC254x chip"},
		{0}
	};
	struct argp argp = {
		.options = options,
		.parser = arg_parser,
		.args_doc = "[firmware.bin]",
		.doc =
		    "Flash firmware on a CC254x chip, like those used in HM-10 modules.",
	};

	argp_parse(&argp, argc, argv, 0, NULL, &args);

	pFile = fopen(args.fName, "rb");
	if (!pFile) {
		perror("Unable to open file");
		return -1;
	}

	fseek(pFile, 0, SEEK_END);
	fSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	if (fSize % 512 != 0)
		fprintf(stderr,
			"Warning: file size isn't a multiple of 512, last bytes will be set to 0xFF\n");

	printf("Firmware loaded, %d kB\n", fSize / 1024);

	if (exportPin(args.DD))
		return -1;
	if (exportPin(args.DC))
		return -1;
	if (exportPin(args.RESET))
		return -1;

	if (ProgrammerInit())
		return -1;

	debug_init();
	chip_id = read_chip_id();
	if (chip_id == 0) {
		fprintf(stderr, "Error: no chip detected.\n");
		return -1;
	} else
		printf("Chip ID = 0x%X\n", chip_id);

	RunDUP();
	debug_init();
	printf("Erasing chip...\n");

	chip_erase();
	printf("Chip erased.\n");
	RunDUP();
	debug_init();

	// Switch DUP to external crystal osc. (XOSC) and wait for it to be stable.
	// This is recommended if XOSC is available during programming. If
	// XOSC is not available, comment out these two lines.
	write_xdata_memory(DUP_CLKCONCMD, 0x80);
	while (read_xdata_memory(DUP_CLKCONSTA) != 0x80) ;
	printf("XOSC has stabilized.\n");

	// Enable DMA (Disable DMA_PAUSE bit in debug configuration)
	debug_command(CMD_WR_CONFIG, &debug_config, 1);

	flash_chip(pFile, fSize, args.verify);

	RunDUP();

	unexportPin(args.DC);
	unexportPin(args.DD);
	unexportPin(args.RESET);

	return 0;
}
