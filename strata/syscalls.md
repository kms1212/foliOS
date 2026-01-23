# System Calls

## For Generic Process

- `[0:000:0000] node_open(in const u8 ptr path, in u32 flags, out u32 handle)`
  - open node
- `[0:000:0001] node_close(in u32 handle)`
  - close node
- `[0:000:0002] node_get_interface_id(in u32 handle, in const uuid ptr if_uuid, in u32 request_ver, out u32 if_id, out u32 result_ver)`
  - get interface handle
- `[0:000:0003] node_call(in u32 handle, in u32 if_id, in u32 func_id, in const opaque ptr args, in u32 arg_size, in opaque ptr result, in u32 result_size)`
  - call function of node interface
- `[0:000:0004] node_bind(in u32 handle, in u32 if_id, in u32 bind_id, in u32 signal_id)`
  - bind signal to node interface
- `[0:000:0005] node_unbind(in u32 handle, in u32 if_id, in u32 bind_id)`
  - unbind signal from node interface

- `[0:001:0000] process_get_current(out u32 handle)`
  - get handle of the current process
- `[0:001:0001] process_exit()`
  - exit process
- `[0:001:0002] process_yield()`
  - yield processor
- `[0:001:0003] process_sleep(in u32 msec)`
  - sleep process for milliseconds
- `[0:001:0004] process_create(in const u8 ptr path, in const char ptr ptr args, in u32 arg_count, in const char ptr ptr envs, in u32 env_count, in const struct ptr attributes, out u32 handle)`
  - create a new child process
- `[0:001:0005] process_duplicate(in const struct ptr attributes, out u32 handle)`
  - duplicate current process
- `[0:001:0006] process_wait(in struct *states, in u32 state_count, in s32 timeout_ms)`
  - wait for selected processes to finish
- `[0:002:0004] process_suspend(in u32 handle)`
  - suspend process
- `[0:002:0005] process_resume(in u32 handle)`
  - resume process

- `[0:002:0000] thread_get_current(out u32 handle)`
  - get handle of the current thread
- `[0:002:0001] thread_exit()`
  - exit thread
- `[0:002:0002] thread_create(in u32 entry, in u32 stack_size, in u32 flags, out u32 handle)`
  - create thread
- `[0:002:0003] thread_wait(in struct *states, in u32 state_count, in s32 timeout_ms)`
  - wait for selected threads to finish
- `[0:002:0004] thread_suspend(in u32 handle)`
  - suspend thread
- `[0:002:0005] thread_resume(in u32 handle)`
  - resume thread

# For Device Driver Process

- `[1:000:0002] node_driver_irq_bind(in u32 handle, in u32 bind_id, in u32 signal_id)`
- `[1:000:0003] node_driver_irq_bind_to_kmfunc(in u32 handle, in u32 bind_id, in u32 kmfunc_id)`
- `[1:000:0004] node_driver_irq_unbind(in u32 handle in u32 bind_id)`
- `[1:000:0005] node_driver_call_kmfunc(in u32 handle, in u32 kmfunc_num, in const opaque ptr args, in u32 arg_size, in opaque ptr result, in u32 result_size)`
- `[1:000:0006] node_create(in const struct ptr devinfo, in u32 user_state_size, out u32 handle)`
- `[1:000:0007] node_remove(in u32 handle)`

# For Filesystem Driver Process

- `[1:001:0000] filesystem_create(in const struct fsinfo, in u32 state_size, out u32 handle)`
- `[1:001:0001] filesystem_remove(in u32 handle)`

# For Bus Driver Process

- `[1:002:0000] bus_create(in const struct businfo, in u32 state_size, out u32 handle)`
- `[1:002:0001] bus_remove(in u32 handle)`

# For Input Method Driver Process

- `[1:003:0000] input_method_create(in const struct businfo, in u32 state_size, out u32 handle)`
- `[1:003:0001] input_method_remove(in u32 handle)`
