# My Gameboy

Gameboy emulator in pure c.

### Modules

```
|--------------|
|              |
|              |
|              |
|              |
|              |
|              |
|              |
|--------------|
```

### Structure

```c
main {
	init cpu
	init internal ram
	init internal rom
	init screen
	init input

	cpu link internal ram
	cpu link internal rom
	screen link internal ram(for video ram)
	screen link cpu(for call interruption function)
	input link cpu(for interruption)

	cpu execute internal ROM

	cart MBC initialization
	cart ram initialization
	cart rom initialization

	loop {
	    screen refresh
	    input refresh
	    cpu run(loop delta t)
	}
}
```