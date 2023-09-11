#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/util/queue.h"

#include "../console/console.h"
#include "../console/terminal.h"

#include "../psram/psram.h"
#include "../cache/cache.h"
#include "../emulator/emulator.h"

#include "f_util.h"
#include "ff.h"

#include "default64mbdtc.h"

#include "../config/rv32_config.h"

static uint32_t HandleException(uint32_t ir, uint32_t retval);
static uint32_t HandleControlStore(uint32_t addy, uint32_t val);
static uint32_t HandleControlLoad(uint32_t addy);
static void HandleOtherCSRWrite(uint8_t *image, uint16_t csrno, uint32_t value);
static uint32_t HandleOtherCSRRead(uint8_t *image, uint16_t csrno);
static int IsKBHit();
static int ReadKBByte();

static uint64_t GetTimeMicroseconds();
static void MiniSleep();

FRESULT loadFileIntoRAM(const char *imageFilename, uint32_t addr);
void loadDataIntoRAM(const unsigned char *d, uint32_t addr, uint32_t size);

#define MINIRV32WARN(x...) console_printf(x);
#define MINIRV32_DECORATE static
#define MINI_RV32_RAM_SIZE (EMULATOR_RAM_MB * 1024 * 1024)
#define MINIRV32_IMPLEMENTATION
#if EMULAOTR_FAF
#define MINIRV32_POSTEXEC(pc, ir, retval)             \
    {                                                 \
        if (retval > 0)                               \
        {                                             \
            console_printf("FAULT\n");                \
            return 3;                                 \
        }                                             \
    }
#else 
#define MINIRV32_POSTEXEC(pc, ir, retval)             \
    {                                                 \
        if (retval > 0)                               \
            retval = HandleException(ir, retval);     \
    }
#endif
#define MINIRV32_HANDLE_MEM_STORE_CONTROL(addy, val) \
    if (HandleControlStore(addy, val))               \
        return val;
#define MINIRV32_HANDLE_MEM_LOAD_CONTROL(addy, rval) rval = HandleControlLoad(addy);
#define MINIRV32_OTHERCSR_WRITE(csrno, value) HandleOtherCSRWrite(image, csrno, value);
#define MINIRV32_OTHERCSR_READ(csrno, rval)      \
    {                                            \
        rval = HandleOtherCSRRead(image, csrno); \
    }

// SD Memory Bus
#define MINIRV32_CUSTOM_MEMORY_BUS

static void MINIRV32_STORE4(uint32_t ofs, uint32_t val)
{
    cache_write(ofs, &val, 4);
}

static void MINIRV32_STORE2(uint32_t ofs, uint16_t val)
{
    cache_write(ofs, &val, 2);
}

static void MINIRV32_STORE1(uint32_t ofs, uint8_t val)
{
    cache_write(ofs, &val, 1);
}

static uint32_t MINIRV32_LOAD4(uint32_t ofs)
{
    uint32_t val;
    cache_read(ofs, &val, 4);
    return val;
}

static uint16_t MINIRV32_LOAD2(uint32_t ofs)
{
    uint16_t val;
    cache_read(ofs, &val, 2);
    return val;
}

static uint8_t MINIRV32_LOAD1(uint32_t ofs)
{
    uint8_t val;
    cache_read(ofs, &val, 1);
    return val;
}

#include "mini-rv32ima.h"

// static void DumpState(struct MiniRV32IMAState *core);
static void DumpState(struct MiniRV32IMAState *core)
{
	unsigned int pc = core->pc;
	unsigned int *regs = (unsigned int *)core->regs;
	uint64_t thit, taccessed;
    uint64_t writes, reads;

	cache_get_stat(&thit, &taccessed);
    RAMGetStat(&reads, &writes);

	console_printf("\x1b[32mCache: hit: %llu, accessed: %llu\n\r", thit, taccessed);
    console_printf("\x1b[32mRAM: read: %llu, write: %llu\n\r", reads, writes);
	console_printf("\x1b[32mPC: %08x\r\n", pc);
}
struct MiniRV32IMAState core;

int rvEmulator()
{
    uint32_t dtb_ptr = MINI_RV32_RAM_SIZE - sizeof(default64mbdtb);
    const uint32_t *dtb = default64mbdtb;

    FRESULT fr = loadFileIntoRAM(IMAGE_FILENAME, 0);
    if (FR_OK != fr)
        console_panic("\r\x1b[31mError loading image: %s (%d)\n", FRESULT_str(fr), fr);
    console_printf("\r\x1b[32mImage loaded sucessfuly!\x1b[m\n\n\r");

    uint32_t validram = dtb_ptr;
    loadDataIntoRAM(default64mbdtb, dtb_ptr, sizeof(default64mbdtb));

    // Tell linux on how much ram we have
    uint32_t dtbRamValue = (validram >> 24) | (((validram >> 16) & 0xff) << 8) | (((validram >> 8) & 0xff) << 16) | ((validram & 0xff) << 24);
    MINIRV32_STORE4(dtb_ptr + 0x13c, dtbRamValue);

    // Setup the Emulator Core
    core.regs[10] = 0x00;                                                // hart ID
    core.regs[11] = dtb_ptr ? (dtb_ptr + MINIRV32_RAM_IMAGE_OFFSET) : 0; // dtb_pa (Must be valid pointer) (Should be pointer to dtb)
    core.extraflags |= 3;                                                // Machine-mode.

    core.pc = MINIRV32_RAM_IMAGE_OFFSET;

    // Start the Emulator
    #if !EMULATOR_FIXED_UPDATE
        uint64_t lastTime = GetTimeMicroseconds() / EMULATOR_TIME_DIV;
    #endif

    while(true) {
        // Check if the H/W trigger is pulled
        if(gpio_get(2) != 1) { console_printf("\x1b[33mH/W Trig Stop!"); DumpState(&core); break; }
        
        // If not, continue the emulator
        uint64_t *this_ccount = ((uint64_t *)&core.cyclel);
        uint32_t elapsedUs = 0;
        #if EMULATOR_FIXED_UPDATE
            elapsedUs = *this_ccount / EMULATOR_TIME_DIV;
        #else
            elapsedUs = GetTimeMicroseconds() / EMULATOR_TIME_DIV - lastTime;
            lastTime += elapsedUs;
        #endif

        int ret = MiniRV32IMAStep(&core, NULL, 0, elapsedUs, EMUALTOR_INSTR_FLIP); // Execute upto 1024 cycles before breaking out.
        switch (ret)
        {
        case 0:
            // Return code 0 means All Good
            break;
        case 1:
            // Return code 1 means WFI (Wait For Intrrupt)
            MiniSleep();
            *this_ccount += EMUALTOR_INSTR_FLIP;
            break;
        case 3:
            // Return code 3 means illegal opcode
            console_panic("\n\x1b[32mEmulator exit with error code 3!");
            break;
        case 0x7777:
            // 0x7777 is the syscon for REBOOT
            console_printf("\n\x1b[32mREBOOT@0x%08x%08x\n", core.cycleh, core.cyclel);
            return EMU_REBOOT; // syscon code for reboot
        case 0x5555:
            // 0x5555 is the syscon for POWEROFF
            console_printf("\n\x1b[32mPOWEROFF@0x%08x%08x\n", core.cycleh, core.cyclel);
            return EMU_POWEROFF; // syscon code for power-off
        default:
            console_printf("\\x1b[31mUnknown failure (%d)!\n", ret);
            return EMU_UNKNOWN;
            break;
        }
    }
    
    // Hardware POWEROFF
    console_printf("\nH/W POWEROFF@0x%08x%08x\n", core.cycleh, core.cyclel);
    return EMU_POWEROFF;
}

//////////////////////////////////////////////////////////////////////////
// Functions for the emulator
//////////////////////////////////////////////////////////////////////////

// Keyboard
static inline int IsKBHit()
{
    if (queue_is_empty(&kb_queue))
        return 0;
    else
        return 1;
}

static inline int ReadKBByte()
{
    char c;
    if (queue_try_remove(&kb_queue, &c))
        return c;
    return -1;
}

// Exceptions handling

static uint32_t HandleException(uint32_t ir, uint32_t code)
{
    // Weird opcode emitted by duktape on exit.
    if (code == 3)
    {
        // Could handle other opcodes here.
    }
    return code;
}

// CSR handling (Linux HVC console)

static inline void HandleOtherCSRWrite(uint8_t *image, uint16_t csrno, uint32_t value)
{
    if( csrno == 0x136 )
	{
		console_printf( "%d", value );
	}
	else if( csrno == 0x137 )
	{
		console_printf( "%08x", value );
	}
	else if( csrno == 0x138 )
	{
		//Print "string"
		uint32_t ptrstart = value - MINIRV32_RAM_IMAGE_OFFSET;
		uint32_t ptrend = ptrstart;
		if( ptrstart >= MINI_RV32_RAM_SIZE )
			console_printf( "\r\n\x1b[31mDEBUG PASSED INVALID PTR (%08x)\r\n", value );
		while( ptrend < MINI_RV32_RAM_SIZE )
		{
			if( MINIRV32_LOAD1(ptrend) == 0 ) break;
            console_putc(MINIRV32_LOAD1(ptrend));
			ptrend++;
		}
	}
    else if (csrno == 0x139)
    {
        console_putc((char)value);
    }
}

static inline uint32_t HandleOtherCSRRead(uint8_t *image, uint16_t csrno)
{
    if (csrno == 0x140)
    {
        if (IsKBHit())
            return ReadKBByte();
        else
            return -1;
    }

    return 0;
}

// MMIO handling (8250 UART)

static uint32_t HandleControlStore(uint32_t addy, uint32_t val)
{
    if (addy == 0x10000000) // UART 8250 / 16550 Data Buffer
        console_putc(val);

    return 0;
}

static uint32_t HandleControlLoad(uint32_t addy)
{
    // Emulating a 8250 / 16550 UART
    if (addy == 0x10000005)
        return 0x60 | IsKBHit();
    else if (addy == 0x10000000 && IsKBHit())
        return ReadKBByte();

    return 0;
}

// Timing
static inline uint64_t GetTimeMicroseconds()
{
    absolute_time_t t = get_absolute_time();
    return to_us_since_boot(t);
}

static void MiniSleep()
{
    #if EMULATOR_WFI_SLEEP
        sleep_ms(1);
    #endif
}

// Memory and file loading

FRESULT loadFileIntoRAM(const char *imageFilename, uint32_t addr)
{
    FIL imageFile;
    FRESULT fr = f_open(&imageFile, imageFilename, FA_READ);
    if (FR_OK != fr && FR_EXIST != fr)
        return fr;

    FSIZE_t imageSize = f_size(&imageFile);

    uint8_t buf[4096];
    while (imageSize >= 4096)
    {
        fr = f_read(&imageFile, buf, 4096, NULL);
        if (FR_OK != fr)
            return fr;
        accessPSRAM(addr, 4096, true, buf);
        addr += 4096;
        imageSize -= 4096;
    }

    if (imageSize)
    {
        fr = f_read(&imageFile, buf, imageSize, NULL);
        if (FR_OK != fr)
            return fr;
        accessPSRAM(addr, imageSize, true, buf);
    }

    fr = f_close(&imageFile);
    return fr;
}

void loadDataIntoRAM(const unsigned char *d, uint32_t addr, uint32_t size)
{
    while (size--) 
        accessPSRAM(addr++, 1, true, (void*) d++);
}
