
#include <gba_console.h>
#include <gba_video.h>
#include <gba_dma.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define 	GBA_IME 	(*(vu32*)0x04000208)
#define 	GBA_IE	 	(*(vu16*)0x04000200)
#define 	GBA_IF	 	(*(vu16*)0x04000134)

#define 	GBA_KEY	 	(*(vu16*)0x04000130)
#define 	GBA_RCNT	(*(vu16*)0x04000134)
#define 	GBA_JOYCNT	(*(vu16*)0x04000140)
#define 	GBA_JOYSND	(*(vu32*)0x04000154)
#define 	GBA_JOYSTAT	(*(vu16*)0x04000158)

#define 	GBA_CNT_L0 	(*(vu16*)0x08000000)
#define 	GBA_CNT_L1 	(*(vu16*)0x08010000)
#define 	GBA_CNT_L2 	(*(vu16*)0x08014000)

#define 	GBA_CNT_H0 	(*(vu16*)0x0A008000)
#define 	GBA_CNT_H1 	(*(vu16*)0x0A010000)
#define 	GBA_CNT_H2 	(*(vu16*)0x0A014000)

#define 	GBA_DATA_8 	((vu8 *)(0x08000000))
#define 	GBA_DATA_16 	((vu16 *)(0x08000000))
#define 	GBA_DATA_32 	((vu32 *)(0x08000000))

#define 	WRAM_16 	((vu16 *)(0x03000000))
#define 	WRAM_32 	((vu32 *)(0x03000000))

#define 	GBA_IDATA_8 	((vu8 *)(0x8008000))
#define 	GBA_IDATA_16 	((vu16 *)(0x8008000))
#define 	GBA_IDATA_32 	((vu32 *)(0x8008000))

u16 data_buffer[0x200];
u16 data_index = 0;

u32 temp = 0;

void send_data(u32 len)
{
	u32 data = 0;

	//Send first word as len of data + 1 for the length data itself
	GBA_JOYSND = 0x00;
	GBA_JOYCNT |= 0x07;
	GBA_JOYSND = len;

	//Wait for send to complete
	while((GBA_JOYCNT & 0x04) == 0) { }
	GBA_JOYCNT |= 0x07;
	
	//Send the rest of data
	for(u32 x = 0; x < len; x++)
	{
		data = data_buffer[x];

		GBA_JOYSND = data;

		while((GBA_JOYCNT & 0x04) == 0) { }
		GBA_JOYCNT |= 0x07;
	}

	GBA_JOYSND = 0;
	
	while((GBA_JOYCNT & 0x04) == 0) { }
	GBA_JOYCNT |= 0x07;

	data_index = 0;

	//Clear buffer for next transfer
	for(u32 x = 0; x < 0x200; x++) { data_buffer[x] = 0; }
}

bool wait_4_io()
{
	u32 t_val = 0;
	u32 limit = 0;	

	while(t_val != 0xAA)
	{
		t_val = GBA_CNT_H1;
		
		u32 hold = (t_val >> 13);
		t_val <<= 16;
		t_val >>= 13;
		t_val |= hold;
		t_val &= 0xFFFF;
		t_val &= 0xAA;

		limit++;
		
		if(limit == 0x100000)
		{
			printf("IO EXIT -> %x\n", t_val);
			return false;
		}
	}

	return true;	
}

void wait_4_button()
{
	bool start = false;

	while(!start)
	{
		if(~GBA_KEY & 0x1) { start = true; }
	}

	start = false;

	while(!start)
	{
		if(GBA_KEY & 0x1) { start = true; }
	}
}

void read_bootstrap()
{
	iprintf("\x1b[2J\x1b[0;0HCampho Dumper 0.5\n");
	printf("Dumping Bootstrap...\n");

	data_index = 0;

	for(u32 x = 0; x < 36; x++)
	{
		data_buffer[data_index++] = GBA_DATA_16[x];
	}

	for(u32 x = 0; x < 62; x++)
	{
		data_buffer[data_index++] = GBA_IDATA_16[x];
	}

	send_data(data_index);
	
	printf("Bootstrap Dump Complete\n");
}

void read_program_rom()
{
	GBA_CNT_L1 = 0xA00A;

	u16 len = 0;
	u16 original_len = 0;
	u16 stat = 0;
	u16 in_data = 0;
	bool is_done = false;

	data_index = 0;

	GBA_CNT_L2 = 0xA00A;

	//Check Loop
	while(!is_done)
	{
		while((GBA_CNT_L1 & 0xA00A) != 0xA00A) { }
		GBA_CNT_L1 = 0xA00A;

		stat = GBA_CNT_L0;
		len = GBA_CNT_L0;
		original_len = len;

		data_buffer[data_index++] = stat;
		data_buffer[data_index++] = len;

		iprintf("\x1b[2J\x1b[0;0HCampho Dumper 0.5\n");
		printf("Dumping P-ROM: %x\n", stat);

		if(stat != 0xCD00)
		{
			while(len)
			{
				data_buffer[data_index++] = GBA_CNT_L0;
				len -= 2;

				if(data_index == 0x200)
				{
					send_data(0x200);
				}
			}

			//Send remaining data
			if(data_index)
			{
				send_data(data_index);
			}
		}

		else { is_done = true; }

		GBA_CNT_L0 = 0;
		GBA_CNT_L0 = 0;
		GBA_CNT_L2 = 0xA00A;
	}

	//Send remaining data
	if(data_index)
	{
		send_data(data_index);
	}

	printf("P-ROM Dump Complete\n");
}

void setup_g_rom(u32 g_rom_addr, u32 g_offset)
{
	GBA_CNT_H1 = 0x4015;

	GBA_CNT_H0 = 0xB742;

	GBA_CNT_H0 = 0x01;

	GBA_CNT_H0 = (g_rom_addr & 0xFFFF);

	GBA_CNT_H0 = (g_rom_addr >> 16);

	GBA_CNT_H0 = (g_offset & 0xFFFF);

	GBA_CNT_H0 = (g_offset >> 16);

	GBA_CNT_H2 = 0x4015;
}

u32 read_manual(u32 g_rom_addr, u32 g_rom_offset)
{
	setup_g_rom(g_rom_addr, g_rom_offset);
	if(!wait_4_io()) { return 1; }

	bool is_header = (g_rom_offset == 0xFFFFFFFF) ? true : false;
	u32 data_size = 0;
	u32 read_size = (is_header) ? 8 : 2048;
	u32 check_size = 0;
	u16 data_val = 0;

	data_index = 0;

	while(data_size < read_size)
	{
		data_val = GBA_CNT_H0;
		data_size++;

		data_buffer[data_index++] = (data_val & 0xFFFF);
		
		if(data_index == 0x200)
		{
			send_data(0x200);
		}

		//Check size - Individual Bank
		if((!is_header) && (data_size == 2))
		{
			read_size = data_val;
			check_size = read_size;

			if(!read_size) { return 2; }

			read_size *= 4;

			printf("Reading G-ROM @ %x\n", g_rom_addr);
			printf("Using G-ROM Offset %x\n", g_rom_offset);
			printf("G-ROM Read Size: %d\n", read_size);

			read_size += 2;
		}

		//Check size - Header
		else if((is_header) && (data_size == 8))
		{
			if((data_buffer[4] == 0xFFFF) && (data_buffer[5] == 0xFFFF))
			{
				return 3;
			}

			if((data_buffer[4] == 0x0000) && (data_buffer[5] == 0x0000))
			{
				return 4;
			}
		}
	}

	if(data_index)
	{
		send_data(data_index);
	}

	if((!is_header) && (read_size < 0x1F4)) { return 2; }

	return 0;
}

int read_graphics_rom()
{
	u32 bank_id = 0x0001;
	u32 banks_done = 1;

	int result = 0;

	while(bank_id < 0x10000)
	{
		u32 bank_offset = 0xFFFFFFFF;
		u32 bank_status = 0;
		
		//Read bank data
		while(bank_status == 0)
		{
			iprintf("\x1b[2J\x1b[0;0HCampho Dumper 0.5\n");
			printf("Dumping G-ROM: %u\n", banks_done);

			bank_status = read_manual(bank_id, bank_offset);

			switch(bank_status)
			{
				//Bank read successfully
				case 0:

					//Header
					if(bank_offset == 0xFFFFFFFF)
					{
						bank_offset = 0;
					}

					//Normal Banks
					else
					{
						bank_offset += 0x1F4;

						if((bank_offset & 0xFFFF) > 0x2000)
						{
							bank_offset += ((bank_offset & 0xF000) << 16);
							bank_offset &= ~0xF000;
						}
					}
 
					break;

				//I/O error, quit for now
				case 1:
					printf("Bad Bank ID -> %x\n", bank_id);
					printf("Bad Offest -> %x\n", bank_offset);
					bank_id = 0x10000;
					result = -1;
					break;

				//Current bank read is finished
				case 2:
					bank_id++;
					banks_done++;
					break;

				//Invalid header, end of bank segment, inc bank
				case 3:
					//Some banks are just no good, bruh
					if((bank_id == 0x2000) || (bank_id == 0x4000))
					{
						bank_id++;
					}

					else
					{
						bank_id &= 0xFF00;
						bank_id += 0x2000;
					}

					break;

				//Zero-length bank, these are invalid?
				case 4:
					bank_id++;
					break;
			}
		}
	}

	return result;
}

void send_command(u16 cmd)
{
	GBA_CNT_H1 = 0x4015;

	GBA_CNT_H0 = cmd;

	GBA_CNT_H0 = 0x0000;

	GBA_CNT_H2 = 0x4015;

	u32 data_size = 0;
	u16 data_val = 0;
	data_index = 0;

	while(data_size < 2048)
	{
		data_val = GBA_CNT_H0;
		data_size++;

		data_buffer[data_index++] = (data_val & 0xFFFF);
		
		if(data_index == 0x200)
		{
			send_data(0x200);
		}
	}

	if(data_index)
	{
		send_data(data_index);
	}

	printf("Camera Command %x\n\n", cmd);
}

int main(void)
{
	GBA_JOYSND = 0x00;

	irqInit();
	consoleDemoInit();

	iprintf("\x1b[0;0HCampho Dumper 0.5\n");
	printf("Press A to start...\n");

	wait_4_button();

	GBA_RCNT = 0xC000;

	GBA_IME = 0x01;
	GBA_IE = 0x01;

	read_bootstrap();
	read_program_rom();
	
	if(read_graphics_rom() == 0)
	{
		printf("Campho ROM Dump Complete\n");
		printf("Have a great day! :)\n");
	}

	while(true)
	{
		VBlankIntrWait();
	}
}
