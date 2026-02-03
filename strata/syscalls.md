# KRT Jump Table

- `krt[0] node_open(in const u8 ptr path, in u32 flags, out u32 handle)`
  - open node
- `krt[1] node_close(in u32 handle)`
  - close node
- `krt[2] node_get_interface_funcid_base(in u32 handle, in const uuid ptr if_uuid, in u32 request_abiver, out u32 funcid_base, out u32 result_abiver)`
  - get interface handle
- `krt[3] node_call(in u32 handle, in u32 funcid, in const opaque ptr args, in u32 args_size, in opaque ptr result, in u32 result_size)`
  - call function of node interface
