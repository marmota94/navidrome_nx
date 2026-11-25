import hashlib
import http.server
import json
import subprocess
import threading
import urllib.parse


class Server(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass  # Do not print authenticated request URLs.

    def do_GET(self):
        url = urllib.parse.urlsplit(self.path)
        query = urllib.parse.parse_qs(url.query)
        expected = hashlib.md5(b"s3cret" + query.get("s", [""])[0].encode()).hexdigest()
        if query.get("u") != ["a user+&"] or query.get("t") != [expected]:
            self.send_error(401)
            return
        endpoint = url.path.removeprefix("/rest/").removesuffix(".view")
        status = 200
        body = {"subsonic-response": {"status": "ok"}}
        if endpoint == "getAlbumList2":
            body["subsonic-response"]["albumList2"] = {"album": [{"id": "id /&", "name": "Album", "artist": "Artist", "coverArt": "cover /&"}]}
            if query.get("size") == ["12"]:
                kind = query.get("type", [""])[0]
                assert kind in ("frequent", "recent", "random")
                body["subsonic-response"]["albumList2"] = {"album": [] if kind == "recent" else [{"id": kind, "name": kind, "artist": "Artist"}]}
        elif endpoint == "search3":
            artists = query.get("artistCount") == ["500"]
            item = "artist" if artists else "song"
            if query.get("albumCount") != ["0"] or query.get("songCount" if artists else "artistCount") != ["0"]:
                self.send_error(400)
                return
            assert query.get("artistCount" if artists else "songCount") == ["500"]
            offset = int(query.get("artistOffset" if artists else "songOffset", ["0"])[0])
            term = query.get("query", [""])[0]
            if term == "empty": count = 0
            elif term == "overflow": count = offset + 500
            else:
                assert term == "dense & /"
                count = 507 if artists else 513
            body["subsonic-response"]["searchResult3"] = {item: [
                {"id": f"{item}-{i}", "name" if artists else "title": f"Name {100000 - i:06d}", "artist": f"Artist {i % 3}", "created": str(i)}
                for i in range(offset, min(offset + 500, count))]}
        elif endpoint == "getArtists":
            body["subsonic-response"]["artists"] = {"index": [{"artist": [{"id": f"artist-{i}", "name": f"Artist {i}"} for i in range(13)]}]}
        elif endpoint == "getArtist":
            assert query.get("id") == ["artist /&"]
            body["subsonic-response"]["artist"] = {"album": [{"id": "id /&", "name": "Album", "artist": "Artist"}]}
        elif endpoint == "getPlaylists":
            body["subsonic-response"]["playlists"] = {"playlist": [{"id": "playlist /&", "name": "Evening mix", "songCount": 2, "coverArt": "playlist-cover", "duration": 360}]}
        elif endpoint == "getPlaylist":
            if query.get("id") != ["playlist /&"]:
                self.send_error(400)
                return
            body["subsonic-response"]["playlist"] = {"name": "Evening mix", "entry": [{"id": "first", "title": "First", "suffix": "flac", "bitRate": 900}, {"id": "second", "title": "Second", "duration": 180}]}
        elif endpoint == "getRandomSongs":
            if query.get("s") == ["partial-salt"]:
                self.send_error(503)
                return
            if query.get("size") != ["12"]:
                self.send_error(400)
                return
            body["subsonic-response"]["randomSongs"] = {"song": [{"id": f"random-{i}", "title": f"Random {i}", "artist": "Artist"} for i in range(15)]}
        elif endpoint == "getAlbum" and query.get("id") == ["overlay-long"]:
            body["subsonic-response"]["album"] = {"song": [
                {"id": f"track-{i}", "title": f"Track {i}"} for i in range(12)
            ]}
        elif endpoint == "getAlbum":
            if query.get("id") != ["id /&"]:
                self.send_error(400)
                return
            body["subsonic-response"]["album"] = {"song": [{"id": "track", "title": "Track", "duration": 185, "suffix": "flac", "bitRate": 980, "samplingRate": 44100, "bitDepth": 24, "channelCount": 2}]}
        elif endpoint == "denied":
            body = {"subsonic-response": {"status": "failed", "error": {"code": 40}}}
        elif endpoint == "getStarred2":
            body["subsonic-response"]["starred2"] = {"album": [{"id": "favorite-album", "name": "Favorite album"}], "song": [{"id": "favorite-track", "title": "Favorite track", "coverArt": "cover /&"}]}
        elif endpoint == "getLyricsBySongId":
            if query.get("id") == ["empty"]:
                body["subsonic-response"]["lyricsList"] = {}
            else:
                body["subsonic-response"]["lyricsList"] = {"structuredLyrics": [{"synced": True, "line": [{"start": 0, "value": "First test line"}, {"start": 2000, "value": "Second test line"}]}, {"line": [{"value": "Another language"}]}]}
        elif endpoint == "getLyrics":
            body["subsonic-response"]["lyrics"] = {"value": "First test line\r\n\nLast test line"}
        elif endpoint == "getCoverArt" and (query.get("id") != ["cover /&"] or query.get("size") != ["128"]):
            self.send_error(400)
            return
        elif endpoint == "missingStatus":
            body = {"subsonic-response": {}}
        elif endpoint == "httpError":
            status = 503
        elif endpoint == "redirect":
            status = 302
        raw = json.dumps(body).encode()
        if endpoint == "stream":
            raw = b"fake mp3"
        elif endpoint == "getCoverArt":
            import base64
            raw = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aZV8AAAAASUVORK5CYII=")
        elif endpoint == "malformed":
            raw = b"not json"
        self.send_response(status)
        if status == 302:
            self.send_header("Location", "/must-not-follow")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)


with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Server) as server:
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        subprocess.run(["/tmp/api_test", f"http://127.0.0.1:{server.server_port}"], check=True)
        subprocess.run(["/tmp/overlay_test", f"http://127.0.0.1:{server.server_port}"], check=True)
    finally:
        server.shutdown()
