-- This is a configuration file for nclangcomplete autocompletion plugin for neovim.
-- https://codeberg.org/aerkkila/nclangcomplete

return {
	['kahto_ffmpeg.c'] = {nil, nil},
	['kahto.c'] = {{'-DHAVE_PNG'}, nil},
	default = {{'-DHAVE_PNG'}, 'kahto.c'},
};
