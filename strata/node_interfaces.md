# Default Node Interfaces

UUID Namespace: 3E30B1DB-32E4-4CD5-999C-02ADFE52C417

## Byte Stream Interface

**UUID**: cbbc8ca1-6584-54a5-9991-09eb5dc7d40d (uuidv5, `strata/interface/bytestream`)

### Functions (ABI revision 0)

- `[BASE+0] Seek(in s64 offset, in u32 whence, out s64 offset)`
- `[BASE+1] Tell(out s64 offset)`
- `[BASE+2] Read(in opaque ptr buf, in u64 size, out u64 result)`
- `[BASE+3] Write(in const opaque ptr buf, in u64 size, out u64 result)`
- `[BASE+4] Sync()`

## Block Interface

**UUID**: 8c11ce10-66d8-5e14-8adf-2b9eef46e2f7 (uuidv5, `strata/interface/block`)

### Functions (ABI revision 0)

- `[BASE+0] GetBlockSize(out u64 size)`
- `[BASE+1] Fetch(in u64 block_num, in opaque ptr buf, in u64 count, out u64 result)`
- `[BASE+2] ClearFetch(in u64 block_num, in opaque ptr buf, in u64 count, out u64 result)`
- `[BASE+3] Release(in u64 block_num, in u32 dirty)`
- `[BASE+4] Flush(in u64 block_num, in u64 count)`
- `[BASE+5] Sync()`

## Video Control Interface

**UUID**: a1010272-9b7e-5557-9483-4877696689e8 (uuidv5, `strata/interface/videocontrol`)

### Functions (ABI revision 0)

- `[BASE+0] SetCurrentMode(in u64 index)`
- `[BASE+1] GetCurrentMode(out u64 index)`
- `[BASE+2] GetModeInfo(in u64 index, out u64 count, in struct ptr infobuf, in u64 infobuf_size)`
- `[BASE+3] GetCurrentModeInfo(in struct ptr infobuf, in u64 infobuf_size)`

## Framebuffer Interface

**UUID**: 1950e1ba-d652-5d4c-bd87-e1de26bf01c0 (uuidv5, `strata/interface/framebuffer`)

### Functions (ABI revision 0)

- `[BASE+0] GetBuffer(out opaque ptr buf, out u64 size)`
- `[BASE+1] Invalidate(in u64 x, in u64 y, in u64 width, in u64 height)`
- `[BASE+2] Flush()`

## Accelerated Graphics Interface

**UUID**: d29f5d68-6661-5f47-a82c-e2063355f7b2 (uuidv5, `strata/interface/acceleratedgraphics`)

### Functions (ABI revision 0)

**TBD**

## Console Interface

**UUID**: 814e596b-4d88-588d-8d37-bf0822320815 (uuidv5, `strata/interface/console`)

### Functions (ABI revision 0)

- `[BASE+0] GetBuffer(out opaque ptr buf, out u64 size)`
- `[BASE+1] Invalidate(in u64 x, in u64 y, in u64 width, in u64 height)`
- `[BASE+2] Flush()`
- `[BASE+3] SetCursorPos(in u64 x, in u64 y)`
- `[BASE+4] GetCursorPos(out u64 x, out u64 y)`
- `[BASE+5] SetCursorVisibility(in u64 visible)`
- `[BASE+6] GetCursorVisibility(out u64 visible)`
- `[BASE+7] SetCursorAttribute(in struct ptr attr)`
- `[BASE+8] GetCursorAttribute(out struct ptr attr)`

68D215A0-AE52-5F20-9E16-C9F7AB280744: HID (emos/interface/hid)
6FA07110-FD4D-5EE4-A825-2FC2A4396DA7: Filesystem (emos/interface/filesystem)
