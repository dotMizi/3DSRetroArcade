#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -Isource source/game.c tests/test_game.c -lm -o build-host/test_game
./build-host/test_game
${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -Isource source/arcade.c tests/test_arcade.c -lm -o build-host/test_arcade
./build-host/test_arcade
${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -Itests/stubs -Isource source/game.c source/arcade.c source/draw.c source/menu_icons.c source/scores.c source/score_ui.c source/delta_intro.c tests/test_app.c -lm -o build-host/test_app
(cd build-host && ./test_app)

${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -Isource source/scores.c tests/test_scores.c -o build-host/test_scores
./build-host/test_scores

${CC:-cc} -std=c11 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -Itests/stubs -Isource source/sound_synth.c source/delta_audio.c tests/test_audio.c -lm -o build-host/test_audio
./build-host/test_audio
