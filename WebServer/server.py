from http.server import BaseHTTPRequestHandler, HTTPServer
from serial import Serial
import time
import json
import sys

# ----- Constants -----
# If HOSTNAME is not set, the server binds to all interfaces
# localhost (127.0.0.1), 'hostname' (often 192.168/16 mask), etc.
# localhost cannot be used for the mobile phone server connection
HOSTNAME = ""                   # Server IP that can be specified. E.g. 192.168.1.122
SERVER_PORT = 8080              # Connected server port 
HTML_FILE = "index.html"        # HTML main index file
FAVICON_FILE = "favicon.ico"    # Web icon file used as 'favicon'
REQ_TIMEOUT = 9                 # Max seconds to wait for a request
SERIAL_DELAY = 0.10             # Delay in seconds between serial data transmission
WAITING_DELAY = 0.05            # Delay in seconds between reading an Arduino response
BAUD_RATE = 115200              # Serial communication baud rate (bps)
# After this amount of polling requests of the same IP and [clear] command, next clears are canceled
CLEAR_AFTER_SAME_IPS = 3        # How many requests to wait for another IP to clear

# HTML document  shown when the file was not found (404 error code)
NOTFOUND_HTML = """
<html>
    <head></head>
    <body style="font-family: 'calibri';">
        <div style="width: 400px; height: 500px; margin: 100px auto;">
            <img src="{favicon}" style="opacity: 0.9; width: 50px; height: 50px;">
            <h3>Error <span style="color: #e54060;">404</span></h3>
            <h3 style="color: #666666;">The requested URL was not found on this server.</h3>
        </div>
    </body>
</html>""".format(favicon=FAVICON_FILE)

class Colors:
    """ Enum class for console text colors """
    BLUE = "\033[94m"
    YELLOW = "\033[93m"
    GREEN = "\033[92m"
    RED = "\033[91m"
    END = "\033[0m"

# ----- Globals -----
arduino = None  # End point object used for the serial communication

# ----- Functions and classes -----
def eprint(*args, **kwargs):
    """ Print server information to STDERR """
    print(*args, file=sys.stderr, **kwargs)

def connect_serial():
    """ Setup serial communication """
    try:
        arduino = Serial(port = "COM4", baudrate = BAUD_RATE, timeout = 0)
        eprint(Colors.GREEN + "COM4 successfully connected" + Colors.END)
        return arduino
    except:
        eprint(Colors.RED + "Cannot open COM4 port (it is used by another process or not available)" + Colors.END)
        return None

def send_data(instr_char, value):
    """ Send data from the server to communicating device """
    # Send the formatted message
    arduino.write(str.encode(instr_char + " " + str(value) + "\n"))
    time.sleep(SERIAL_DELAY)
    return("[" + instr_char + " " + str(value) + "]")

def detect_arduino_commands(msg):
    """ Extract the commands and values from the message and send to the serial port """
    # Detect a 'h' command in the JSON (move to home location)
    if("h" in msg):
        eprint("[h] move home")
        # Send move-home instruction to serial communication listener
        arduino.write(b"h\n")
        time.sleep(SERIAL_DELAY)
    # Move the motors up/down/left/right on the 2D plane
    elif("m" in msg):
        eprint(send_data("m", msg["m"]))
    # Save a new home location command
    elif("s" in msg):
        eprint(send_data("s", msg["s"]))
    # If the JSON includes all the coordinates and the type (of a point)
    elif("x" in msg and "y" in msg and "t" in msg):
        eprint(send_data("x", msg["x"]), end = "")
        eprint(send_data("y", msg["y"]), end = "")
        eprint(send_data("t", msg["t"]))
    else:
        eprint("Incorrect message received")

class ServerHandler(BaseHTTPRequestHandler):
    """ HTTP request server handler """

    received_msg = None # Static variable with the message received from a serial communicating device

    # ┌──── Optional Multi-client canvas updating 
    saved_lines = None  # Shared list of line points, which should be on all client canvases 
    clear_lines = False # Bool if a client forced to clear lines on another canvas 
    last_IP = ""        # IP of a client, whose canvas should be cleared
    clear_req_IP = ""   # IP of a client who began the 'clear canvas' command
    same_ip_counter = 0 # Counter for the same IP requests for [clear] command 
    # └────

    def setup(self):
        """ Startup settings """
        BaseHTTPRequestHandler.setup(self)
        # Timeout to stop waiting for invalid requests 
        self.request.settimeout(REQ_TIMEOUT)
    
    def log_message(self, format, *args):
        """ Override the server log printing """
        eprint(Colors.BLUE + self.address_string() + " - ", end = "")
        eprint(format % args + " " + Colors.END)

    # ┌──── Optional Multi-client canvas updating 
    def update_lines(self, msg_raw):
        """ Update shared lines, reduce duplicated lines and send to another clients """
        # If the message is from the last client who tried to '[clear]' canvas, reset clearing predicate
        # At least one IP (ServerHandler.last_IP) was cleared => guaranteed dual client proper functioning 
        if(ServerHandler.clear_req_IP == self.address_string() and ServerHandler.last_IP != ""):
            ServerHandler.clear_lines = False
            ServerHandler.clear_req_IP = ""
        # Ensuring that tha canvas is cleared even when there are one client (IP) with more connections (e.g. Browsers)
        elif(ServerHandler.clear_req_IP == self.address_string() and ServerHandler.clear_lines):
            ServerHandler.same_ip_counter = ServerHandler.same_ip_counter + 1
            # When the counter is > requests from the same IP, reset clear bool for others
            if(ServerHandler.same_ip_counter > CLEAR_AFTER_SAME_IPS):
                ServerHandler.clear_lines = False
                ServerHandler.same_ip_counter = 0
            # Ignore incoming requests after [clear] from the same host 
            ServerHandler.saved_lines = None
            msg_raw = "[]"


        # If a client forces to delete canvas, save their IP and reset stored lines
        if(msg_raw.startswith("[clear]")):
            # Someone else's canvas will be cleared
            ServerHandler.clear_lines = True
            ServerHandler.clear_req_IP = self.address_string()
            ServerHandler.last_IP = ""
            ServerHandler.saved_lines = None

        # If a client is not the one, who called clear-canvas, but should clear it, reset saved lines
        if(ServerHandler.clear_lines and ServerHandler.clear_req_IP != self.address_string()):
            ServerHandler.saved_lines = None
            # Client IP indicates, that the removing can be stopped with a next request of the initial caller
            ServerHandler.last_IP = self.address_string()
        # The received message is neither blank list nor clear command => it's a list of points
        elif(not msg_raw.startswith("[]") and not msg_raw.startswith("[clear]")):
            # E.g. "[{'x':0,'y':0,'pointType':'start'},...,{'x':1,'y':1,'pointType':'end'}]"
            # Delete signs '[' and ']' and replace ',' between objects to differ it from
            # commas between JSON values 
            tempMsg = msg_raw.replace("},{", "}_{")[1:-1]
            # Split the message to STRING objects (but in JSON form) into a list
            tempMsg = list(tempMsg.split("_"))

            # Unite with the stored ones, if it's not the same list
            if(ServerHandler.saved_lines != None and ServerHandler.saved_lines != tempMsg):
                ServerHandler.saved_lines = ServerHandler.saved_lines + tempMsg

                # Indexes of points, which are redundant (whole duplicit lines)
                i_remove = []
                # JSON objects are kept as strings to easily get the unique points of the list
                for i, strJson in enumerate(ServerHandler.saved_lines):
                    # Find out if a point is also again in the rest of the list 
                    if(i+1 < len(ServerHandler.saved_lines) and strJson in ServerHandler.saved_lines[i+1:]):
                        # Duplicit points must shape entire line
                        if(len(i_remove) == 0 and "start" in strJson):
                            i_remove.append(i)
                        elif(len(i_remove) != 0):
                            i_remove.append(i)
                            # The last point causes replacement of needless points with '#' to be deleted later
                            if("end" in strJson):
                                for j in i_remove:
                                    ServerHandler.saved_lines[j] = "#"
                                i_remove = []
                        else:
                            i_remove = []
                # Pick just the points, that left (remove '#' points)
                ServerHandler.saved_lines = [ x for x in ServerHandler.saved_lines if "#" not in x ]
            else:
                ServerHandler.saved_lines = tempMsg
        
        # Default response if an empty list of points "[]"
        response_str = "[]"
        if(ServerHandler.saved_lines != None):
            # Concatenate points back to a string of the incoming format
            response_str = "["+",".join(ServerHandler.saved_lines)+"]"
        eprint("List of JSON lines processed")
        return response_str
    # └────

    def do_GET(self):
        """ Response to GET request from a user to the server (load source files) """
        try:
            file_to_open = self.path    # Requested file from URL (http://HOST:PORT/file_to_open)
            file_content = None

            # If a request is a diretory, select the main HTML file as a response 
            if(file_to_open == "/"):
                file_to_open = "/" + HTML_FILE
            # Response to a favicon request
            elif(file_to_open == "/" + FAVICON_FILE):
                self.send_response(200)
                self.send_header("Content-Type", "image/x-icon")
                # Append header CRLF terminators
                self.end_headers()
                # HTTP response body 
                self.wfile.write(open(FAVICON_FILE, "rb").read())

                eprint(FAVICON_FILE + " loaded")
                return
            
            try:
                # Detach the root directory symbol '/'
                file_to_open = file_to_open[1:]
                file_content = open(file_to_open, encoding = "utf-8").read()
                self.send_response(200) # OK code
            except:
                # Direct HTML in a string as a not-found-page
                file_content = NOTFOUND_HTML
                self.send_response(404) # NOT FOUND code

            eprint(file_to_open + " loaded")

            # Append CRLFs
            self.end_headers()
            # HTTP response body
            self.wfile.write(bytes(file_content, "utf-8"))
        except:
            eprint(Colors.RED + self.address_string() + " - Host connection was canceled" + Colors.END)

    def do_POST(self):
        """ Response to POST request sending web data to the server """
        # Text content received as a client request
        msg_raw = self.rfile.read(int(self.headers["Content-Length"])).decode("ascii")

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()

        # ┌──── Optional Multi-client canvas updating 
        # Update canvas change on all devices
        # The message is a list of string points 
        # e.g. "[{'x':0,'y':0,'pointType':'start'},{'x':1,'y':1,'pointType':'end'},...]"
        if(msg_raw[0] == '['):
            response_str = self.update_lines(msg_raw)
            self.wfile.write(bytes(response_str, "utf-8"))
            return
        # └────

        self.wfile.write(bytes("Data: " + msg_raw, "utf-8"))
        self.wfile.write(b"\nReceived successfully")

        global arduino
        # Received message is disassembled into a JSON object
        msg = json.loads(msg_raw)

        # Detect a 'c' command in the JSON (reconnect COM4 port)
        if("c" in msg):
            eprint("COM4 reconnection")
            arduino = connect_serial()
            return
        
        if(arduino is None):
            eprint(Colors.YELLOW + "Device on COM4 is not available for serial communication" + Colors.END)
            return
        
        # Wait for the Arduino response '1\n' so that the server can send another data
        while(True):
            ServerHandler.received_msg = arduino.readline().decode("utf-8")
            if(len(ServerHandler.received_msg) > 0 and ServerHandler.received_msg[0] == '1'):
                break
            time.sleep(WAITING_DELAY)
        
        # Detect and send all the possible commands received from the web
        detect_arduino_commands(msg)


# ----- Main -----
if(__name__ == "__main__"):
    # Try to connect the serial communication 
    arduino = connect_serial()
    # Create a server
    server = HTTPServer((HOSTNAME, SERVER_PORT), ServerHandler)

    if(HOSTNAME):
        eprint("Server started at http://", HOSTNAME, ":", SERVER_PORT, sep = "")
    else:
        eprint("Server binded to all interfaces on port ", SERVER_PORT, sep = "")

    try:
        # Run the server until the CTRL+C SIGINT is received
        server.serve_forever()
    except KeyboardInterrupt:
        server.server_close()
        eprint("Server stopped")
