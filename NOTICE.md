# Notice

The MIT license in [LICENSE](LICENSE) covers what is ours in this repository:
the host, the lift driver, the setup scripts, the tools and the documentation.
It does not cover the game.

*Afterlife* is © 1996 LucasArts Entertainment Company, and is not ours to
license. Its executables, data, movies, music and art are not in this
repository and are excluded by `.gitignore`.

- You need your own copy of the game to use any of this. Setup links or copies
  your copy into `game/`; the pipeline reads it from there.
- Lifted code is a derivative work of the binary it came from. Anything the
  pipeline writes into `work/` or `src/recomp/gen*/` carries whatever license
  the original carries, not this one. The MIT grant stops at our own work.
- The host runs on [pcrecomp](https://github.com/sp00nznet/pcrecomp)'s
  `native32` runtime (MIT), fetched beside this repository, not copied in.
  The host follows the [Sim Theme Park
  recomp](https://github.com/sp00nznet/simthemepark-recomp), ours and MIT.

