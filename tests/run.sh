#!/usr/bin/env bash
set -euo pipefail
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/api_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/api_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/overlay_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/overlay_test
python3 tests/run.py
g++ -std=c++17 -Isrc -isystem vendor/aurore/library/include/aurore/extern tests/artwork_test.cpp -o /tmp/artwork_test
/tmp/artwork_test
python3 tests/i18n_catalog_test.py
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/settings_test.cpp -ljansson -o /tmp/settings_test
/tmp/settings_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/remote_test.cpp -ljansson -pthread -o /tmp/remote_test
/tmp/remote_test
gcc -std=c99 -Wall -Wextra -Werror -c vendor/qrcodegen/qrcodegen.c -o /tmp/qrcodegen.o
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/qr_test.cpp /tmp/qrcodegen.o -o /tmp/qr_test
/tmp/qr_test
python3 - <<'PY'
import subprocess
actual = subprocess.check_output(['zbarimg', '--quiet', '--raw', '/tmp/pairing-qr.pgm'], stderr=subprocess.DEVNULL).decode().strip()
assert actual == 'http://192.168.1.91:8080/#0123456789abcdef0123456789abcdef'
print('QR scan round trip passed')
PY
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/login_test.cpp -ljansson -o /tmp/login_test
/tmp/login_test
g++ -std=c++17 -isystem vendor/aurore/library/include/aurore/extern tests/font_test.cpp -o /tmp/font_test
/tmp/font_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/waveform_test.cpp -pthread -o /tmp/waveform_test
/tmp/waveform_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lyric_emphasis_test.cpp -o /tmp/lyric_emphasis_test
/tmp/lyric_emphasis_test

g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/song_cache_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/song_cache_test
/tmp/song_cache_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lyrics_navigation_test.cpp -o /tmp/lyrics_navigation_test
/tmp/lyrics_navigation_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/warp_test.cpp -o /tmp/warp_test
/tmp/warp_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/cover_flow_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/cover_flow_test
/tmp/cover_flow_test
g++ -std=c++17 -Wall -Wextra -Werror -DNAVIDROME_VERSION='"1.1.0"' -Isrc tests/release_update_test.cpp -lcurl -ljansson -lmbedcrypto -pthread -o /tmp/release_update_test
/tmp/release_update_test
g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/equalizer_test.cpp -o /tmp/equalizer_test
/tmp/equalizer_test
