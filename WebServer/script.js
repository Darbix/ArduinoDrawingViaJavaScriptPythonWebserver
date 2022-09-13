// ----- Constants -----
const URL = window.location.href;   // URL of the file used for a server
const BORDER_WIDTH = 10;            // Canvas border thickness
const PEN_COLOR = "black";          // Canvas pen color
const DOT_COLOR = "#ff4050";        // Canvas points-to-send color
const STROKE_SIZE = 7;              // Pen size
// HTML element created when the server is not responding
const NOT_RESPONDING_SPAN = "⚠<span id=\"responseText\">Server is not responding</span>";
// ┌──── Optional Multi-client canvas updating 
const POLLING_PING_DELAY = 600      // Miliseconds to wait until the next canvas update is sent
// └────

const WIDTH = 650;                  // Canvas width
const HEIGHT = 650;                 // Canvas height

// ----- Global variables -----
let pointType = "mid";              // Type of the current line point (start, mid, end)
let lastX = 0;                      // Last X coord detected
let lastY = 0;                      // Last Y coord detected

// ┌──── Optional Multi-client canvas updating 
let awaitResponse = false;          // State lock to restrict saving into 'lines' before it's updated
let lines = []                      // List of all line points shared via the server  
let tempLines = []                  // Temporary lines (points) currently being drawn before they can be saved
// └────

// ----- Server communication overload prevention -----
let SIFT_QUANTITY = 9;              // Only every N-th stroke point will be detected and send (default value)
let SHOW_POINTS = false;            // Show line points which are actually sent 

// Set the default SIFT_QUANTITY to HTML
let inputSiftQuant = document.querySelector("#inputSiftQuant");
inputSiftQuant.value = SIFT_QUANTITY;

// http(s):
if(window.location.protocol.startsWith("http"))
    changeStatus("• online", "#07c210");
// file:
else 
    changeStatus("• offline", "red");


// ----- Global functions -----
/**
 * Change the server status (online, offline) and the status color
 * @param {String} state Status name
 * @param {String} color Color of the state
 */
function changeStatus(state, color){
    let statusBar = document.querySelector("#statusBar");
    statusBar.innerHTML = state;
    statusBar.style.color = "#22222D"
    statusBar.style.background = color;
}

/**
 * Clear the whole canvas
 */
function clearCanvas(){
    const canvas = document.querySelector("#canvas");
    canvas.getContext("2d").clearRect(0, 0, canvas.width, canvas.height);

    // ┌──── Optional Multi-client canvas updating 
    if(lines.length != 0){
        sendJson("[clear]");
        lines = [];
    }
    // └────
}

/**
 * Update the chosen sift quantity from the user input
 */
function siftQuantChanged(){
    const inputSiftQuant = document.querySelector("#inputSiftQuant");
    SIFT_QUANTITY = inputSiftQuant.value;
}

/**
 * Update the chosen value, if a user wants to show the sent points
 */
function showPointsChanged(){
    const inputPoints = document.querySelector("#inputPoints");
    SHOW_POINTS = inputPoints.checked;
}

/**
 * Send a simple HTTP request with the JSON content (for button clicks)
 * @param {String} data  JSON string structured text
 * @returns JSON data sent
 */
function sendJson(data){
    // HTTP request sending the command to move to the home position
    let xhttp = new XMLHttpRequest();
    xhttp.open("POST", URL);

    xhttp.setRequestHeader("Accept", "application/json");
    xhttp.setRequestHeader("Content-Type", "application/json");

    xhttp.onerror = function(){
        let responseBar = document.querySelector("#responsebar");

        responseBar.innerHTML = NOT_RESPONDING_SPAN;
        setTimeout(() => {
            responseBar.innerHTML = "";
        }, 2000)
    };

    // ┌──── Optional Multi-client canvas updating 
    xhttp.onreadystatechange=function(){
        if (xhttp.readyState == 4 && xhttp.status == 200) {
            // If the response is a list "[{x,y,pointType},...]" haven't reacted yet
            if(xhttp.responseText[0] == '['){
                // If the line list is empty, clear canvas
                if(xhttp.responseText.startsWith("[]")){
                    // If someone else cleared the canvas (command from the server, not own)
                    if(lines.length != 0 && tempLines.length == 0){
                        lines = []
                        clearCanvas();
                    }
                }
                // Else parse the list into JSON points representing lines
                else{
                    lines = JSON.parse(xhttp.responseText);
                    drawStoredLines();
                }
                // Try to re-save interrupted line saving
                if(tempLines.length != 0 && tempLines[tempLines.length-1]["pointType"] == "end"){
                    lines = lines.concat(tempLines);
                    tempLines = [];
                }
                // The request-response cycle is done
                awaitResponse = false;
            }
        }
    }; 
    // └────

    xhttp.send(data);
    return data;
}

/**
 * Send a request to save a new home location
 * @param {String} id Button id
 */
function saveHome(id){
    console.log(sendJson("{\"s\": \"save\"}"));
    let saveHomeButton = document.getElementById(id);
    // Blink the save home symbol
    saveHomeButton.innerHTML = "◉";
    setTimeout(() => {
        saveHomeButton.innerHTML = "◎";
    }, 1000);
}

/**
 * Send a request to move the pen to the home location
 */
function moveToHome(){
    console.log("%c" + sendJson("{\"h\": \"home\"}"), "color: #ff3300;")
}

/**
 * Send a request to reconnect to COM4 port for serial communication
 */
 function connect(){
    console.log("%c" + sendJson("{\"c\": \"connect\"}"), "color: #ff3300;")
}

/**
 * Send a request to move the pen to set a new home location
 * @param {String} direction Movement direction to go in
 */
function move(direction){
    console.log(sendJson("{\"m\": \"" + direction + "\"}"));
}

// ┌──── Optional Multi-client canvas updating 
/**
 * Change the state of multi-client connection to server (canvas update sending)
 * @param id Toggle button string id
 */
function switchMulticlient(id){
    // Static polling state var, if a multiclient connection is on (!= null), off (null)
    if (typeof switchMulticlient.polling == 'undefined')
        switchMulticlient.polling = null;
    // Switch button
    var btn = document.getElementById(id);

    // If the update sharing is on, turn it off
    if(switchMulticlient.polling != null){
        clearInterval(switchMulticlient.polling);
        switchMulticlient.polling = null;

        // Make the button darken
        btn.style.opacity = "0.4"; 
    }
    else{
        // Every POLLING_PING_DELAY [ms] send POST request to update canvas
        switchMulticlient.polling = window.setInterval(() => {
            // Kind of MUTEX to lock the line saving, before a response comes
            awaitResponse = true;
            sendJson(JSON.stringify(lines));
        }, POLLING_PING_DELAY);

        btn.style.opacity = "1";
    }
}

/**
 * Draw all the lines currently being drawn (tempLines) and saved (lines)
 */
function drawStoredLines(){
    const canvas = document.querySelector("#canvas");
    const ctx = canvas.getContext("2d"); 

    // Concatenate the whole saved lines with probably not done yet tempLines
    let storedLines = lines.concat(tempLines);

    if(storedLines != null && storedLines.length != 0){
        ctx.lineWidth = STROKE_SIZE;
        ctx.lineCap = "round";

        // Clear the canvas
        ctx.clearRect(0, 0, canvas.width, canvas.height);

        drawLines(ctx, storedLines);
    }
}
/**
 * Draw all lines from the list of points on canvas
 * @param {Object} ctx Canvas context
 * @param {Array} list list of line points {x,y,pointType}
 */
function drawLines(ctx, list){
    if(list != null && list.length != 0){
        ctx.beginPath();
        for(let i = 0; i < list.length; i++){
            if(list[i]["pointType"] != "end"){
                ctx.lineTo(list[i]["x"], list[i]["y"]);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(list[i]["x"], list[i]["y"]);
            }
            else
                // End the current line (prepare for next)
                ctx.beginPath();
        }
        ctx.beginPath();
    }
}
// └────


// ----- Event handling -----
// Process events after the window load 
window.addEventListener("load", () => {
    const canvas = document.querySelector("#canvas");   // Canvas object
    const ctx = canvas.getContext("2d");                // Graphic canvas context

    canvas.width = WIDTH;
    canvas.height = HEIGHT;

    let painting = false;   // To prevent drawing when only a mouse is hovering above the plane

    // ----- Auxiliary functions -----
    /**
     * Draw a small circle on the points, which are sent as requests to the server
     * @param {Number} x X coordination of the point
     * @param {Number} y Y coordination of the point
     * @param {Boolean} isEnd True if the point is the line end
     */
    function drawCircle(x, y, isEnd){
        // Begin path will be already started from the start/mid point
        ctx.arc(x, y, STROKE_SIZE+1, 0, 2 * Math.PI, false);
        ctx.fillStyle = DOT_COLOR;
        ctx.fill();
        // Begin the path for the next line to keep it continuing without a gap
        ctx.beginPath();

        // Do not use this point as a start for the next line, if it is an 'endpoint'
        if(!isEnd)
            ctx.moveTo(x, y);
    }

    /**
     * Get the normalized and capped float number from the integer canvas coordination
     * @param {Number} val Integer value
     * @param {Number} maxVal Max value the integer number can reach (max coordination value)
     * @param {Number} decPlaces Decimal places to round to
     * @returns String represented float number in a range 0.0000 - 1.0000 (4 dec digits)
     */
    function normalize(val, maxVal, decPlaces){
        return Math.max(Math.min(val.toFixed(decPlaces) / maxVal, 1.0), 0).toFixed(decPlaces);
    }

    /**
     * Send HTTP POST request with the JSON data containing X, Y coords and the point type
     * @param {Number} x X coordination of the point
     * @param {Number} y Y coordination of the point
     * @param {String} pointType Point type on a line (start, mid or end)
     * @returns Data sent to the server
     */
    function sendRequest(x, y, pointType){
        // Clamp and normalize the coordinates to float values in the range 0.0000 / 1.0000
        let fracX = normalize(x, WIDTH, 6);
        let fracY = normalize(y, HEIGHT, 6);

        // HTTP request sending the values of the exact coordinates
        data = "{\"x\": " + fracX + ", \"y\": " + fracY + ", \"t\": \"" + pointType + "\"}"
        sendJson(data);
        return data;
    }

    // ----- Main functions -----
    /**
     * Start the line after a mouse click (or touch)
     * @param {Event} e 
     */
    function startPosition(e){
        painting = true;
        pointType = "start";
        draw(e);
    }

    /**
     * End the line after a mouse click release
     * @param {Event} e 
     */
    function endPosition(e){
        painting = false;
        pointType = "end";
        draw(e);
    }

    /**
     * Draw on the canvas after a move event is fired
     * @param {Event} e 
     */
    function draw(e){
        // Disables touch actions like swipe-to-refresh, etc.
        e.preventDefault();

        ctx.lineWidth = STROKE_SIZE;
        ctx.lineCap = "round";

        let x = lastX;
        let y = lastY;

        // Get the current x, y point values for the touch controlled devices
        if(e.type == "touchstart" || e.type == "touchmove"){
            x = e.touches[0].clientX - canvas.offsetLeft - BORDER_WIDTH;
            y = e.touches[0].clientY - canvas.offsetTop - BORDER_WIDTH;
            // note: Touchend event on phones does not carry the coordinates with it
        }
        // Get the current x, y point values for the mouse controlled devices
        else if(e.type.startsWith("mouse")){
            x = e.clientX - canvas.offsetLeft - BORDER_WIDTH;
            y = e.clientY - canvas.offsetTop - BORDER_WIDTH;
        }

        // Update the values on the web panel
        valX = document.querySelector("#valX");
        valX.innerHTML = normalize(x, WIDTH, 4);

        valY = document.querySelector("#valY");
        valY.innerHTML = normalize(y, HEIGHT, 4);

        // Do not draw if a mouse just moves over the canvas
        if(!painting && pointType != "end")
            return;

        if(pointType != "end"){
            // Draw a line to the point
            ctx.lineTo(x, y);
            
            ctx.stroke();
            ctx.strokeStyle = PEN_COLOR;

            // Start a new line
            ctx.beginPath();
            // Move to the next start at the current position
            ctx.moveTo(x, y);
        }
        else
            ctx.beginPath(); // Prepare the path for the next line

        // ----- Line point interpolation -----
        // Not all the line points are sent to not overload a communication device and Arduino
        // The number of points on a line, which are actually sent depends on SIFT_QUANTITY

        // Static variable counting the 'mid' points, that should be sent
        if(typeof(draw.counter) == "undefined")
            draw.counter = 0;
        if(pointType == "mid")
            draw.counter = (++draw.counter) % SIFT_QUANTITY; // Circular counter
        else
            draw.counter = 0;   // if the value is 0, the point will be sent
        
        // Start, end and some mid points are sent (when the counter is 0)
        if(draw.counter == 0){
            console.log("%c" + sendRequest(x, y, pointType), "color: #0075ff;");
            // If a user checked that they wants to see these sent point, draw them with a different color
            if(SHOW_POINTS)
                drawCircle(x, y, pointType == "end");
        }

        // Reset for the next point
        lastX = x;
        lastY = y;

        // ┌──── Optional Multi-client canvas updating 
        // Conversion of eventual float values to integers
        // On a touchscreen the values would else get to floats with tens decimal places  
        x = parseInt(x);
        y = parseInt(y);

        // Save a current point to a list of not yet saved lines
        tempLines.push({x,y,pointType});

        // Draw a current line, otherwise it can be cleared in drawStoredLines
        drawLines(ctx, tempLines);

        // If this point is the end of a line, append the line to saved 'lines'
        // If the 'lines' are jhust being sent to the server and AJAX is waiting for a response (awaitResponse is true),
        // do not clear and save tempLines, cause the next attached line end will do it. server response is async and
        // updates 'lines' so the new one would disappear. Without this verification the behaviour is UNPREDICTABLE!
        if(pointType == "end" && awaitResponse == false){
            lines = lines.concat(tempLines);
            tempLines = [];
        }
        // └────

        pointType = "mid";
    }
    
    // ----- Canvas event listeners -----
    canvas.addEventListener("mousedown", startPosition);
    canvas.addEventListener("touchstart", startPosition);

    canvas.addEventListener("mouseup", endPosition);
    canvas.addEventListener("touchend", endPosition);

    canvas.addEventListener("mousemove", draw);
    canvas.addEventListener("touchmove", draw);
});
