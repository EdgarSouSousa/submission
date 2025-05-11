from http.server import BaseHTTPRequestHandler, HTTPServer
import json

HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 8000       # Port to match ESP32's HTTP_BASE_URL

class ESP32Handler(BaseHTTPRequestHandler):
    def _send_response(self, code, message):
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps({"response": message}).encode())

    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        body = self.rfile.read(content_length).decode()

        # Print raw body content
        print(f"[RAW BODY] {body}")

        try:
            data = json.loads(body) if body else {}  # Load JSON if body exists, else empty dict
        except json.JSONDecodeError:
            self._send_response(400, "Invalid JSON")
            return

        if self.path == '/api/temperature':
            print(f"[DATA] Value: {data.get('value')}, Timestamp: {data.get('timestamp')}")
            self._send_response(200, "Data received")

        elif self.path == '/api/ping':
            print("[PING] Received ping")
            self._send_response(200, "pong")
            print("[PING] Responded with pong")

        elif self.path == '/api/error':
            print(f"[ERROR] {data.get('error')}")
            self._send_response(200, "Error logged")

        else:
            self._send_response(404, "Unknown endpoint")


def run():
    server = HTTPServer((HOST, PORT), ESP32Handler)
    print(f"HTTP server running on http://{HOST}:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server.")
        server.server_close()

if __name__ == '__main__':
    run()