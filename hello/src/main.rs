use std::io::Write;
use vitasdk_sys::*;

const W: usize = 960;
const H: usize = 544;

fn main() {
    let _ = std::fs::create_dir_all("ux0:data/ngu");
    if let Ok(mut f) = std::fs::File::create("ux0:data/ngu/log.txt") {
        let _ = writeln!(f, "[ngu_hello] Rust std on Vita OK");
    }
    unsafe {
        let size = 2 * 1024 * 1024; // 960*544*4 rounded to 1 MiB pages
        let blk = sceKernelAllocMemBlock(
            b"fb\0".as_ptr() as *const _,
            SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
            size,
            core::ptr::null_mut(),
        );
        let mut base: *mut core::ffi::c_void = core::ptr::null_mut();
        sceKernelGetMemBlockBase(blk, &mut base);
        let fb = base as *mut u32;
        let frame = SceDisplayFrameBuf {
            size: core::mem::size_of::<SceDisplayFrameBuf>() as u32,
            base,
            pitch: W as u32,
            pixelformat: SCE_DISPLAY_PIXELFORMAT_A8B8G8R8,
            width: W as u32,
            height: H as u32,
        };
        let mut t: u32 = 0;
        loop {
            let c = 0xFF000000 | (t & 0xFF) << 8 | 0x00FF0000 >> 8 & 0;
            for i in 0..W * H {
                *fb.add(i) = c | 0x80;
            }
            sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
            sceDisplayWaitVblankStart();
            t += 1;
            if t > 600 { break; }
        }
    }
}
