'using strict';

var App = function() {

    this.worker = new Worker('worker.js');
    this.collisions = null;
    this.culled = null;
    this.start_time = performance.now();

    // This flag enables us to avoid queuing up work when collision takes
    // longer than a single frame.
    this.pending_collision = false;

    this.worker.onmessage = function(msg) {
        this.pending_collision = false;
        var current = performance.now();
        var time = Math.floor(current - this.start_time);
        this.collisions = new Uint32Array(msg.data.collisions.buffer);
        this.culled = new Uint32Array(msg.data.culled.buffer);
        this.dirty_draw = true;
    }.bind(this);

    var canvas = document.getElementsByTagName('canvas')[0];
    this.winsize = new Float32Array(2);
    var width = this.winsize[0] = canvas.clientWidth;
    var height = this.winsize[1] = canvas.clientHeight;
    this.send_message('d3cpp_set_winsize', this.winsize);

    this.viewport = new Float32Array(4);

    var count = 500, rsize = 0.02, msize = 0.1, i, j, cx, cy, w, h,
        randomX = d3.random.normal(this.winsize[0] / 2, this.winsize[0] / 5),
        randomY = d3.random.normal(this.winsize[1] / 2, this.winsize[1] / 5);
    this.data = new Float32Array(count * 4);
    for (i = 0, j = 0; i < count; i++) {
        cx = randomX();
        cy = randomY();
        h = w = this.winsize[0] * rsize * (msize + Math.random());
        this.data[j++] = cx - w;
        this.data[j++] = cy - h;
        this.data[j++] = cx + w;
        this.data[j++] = cy + h;
    }
    this.send_message('d3cpp_set_data', this.data);

    var x = this.xform = d3.scale.linear()
        .domain([0, width])
        .range([0, width]);

    var y = this.yform = d3.scale.linear()
        .domain([0, height])
        .range([height, 0]);

    var onzoom = this.zoom.bind(this);

    var zoomer = this.mouse_handler = d3.behavior.zoom()
        .x(this.xform)
        .y(this.yform)
        .scaleExtent([1, 40])
        .on("zoom", onzoom);

    document.getElementById("home").onclick = function() {
        d3.transition().duration(750).tween("zoom", function() {
            var ix = d3.interpolate(x.domain(), [0, width]),
                iy = d3.interpolate(y.domain(), [0, height]);
            return function(t) {
              zoomer.x(x.domain(ix(t))).y(y.domain(iy(t)));
              onzoom();
            };
        });
    };

    var resizeThrottle;
    window.onresize = function() {
        clearTimeout(resizeThrottle);
        resizeThrottle = setTimeout(this.refresh_viewport.bind(this), 250);
    }.bind(this);

    this.context = d3.select("canvas")
        .call(this.mouse_handler)
        .node().getContext("2d");

    this.refresh_viewport();

    this.tick = this.tick.bind(this);
    this.tick();

    var url = 'http://broadphase.net/monolith.0000.partial.txt';
    this.fetch(url, function(arraybuf) {
        this.send_message('d3cpp_set_monolith', arraybuf)
    }.bind(this));
};

App.prototype.refresh_viewport = function() {
    var pixelScale = this.pixelScale = window.devicePixelRatio;
    var canvas = document.getElementsByTagName('canvas')[0];
    var width = this.winsize[0] = canvas.clientWidth;
    var height = this.winsize[1] = canvas.clientHeight;
    canvas.width = width * pixelScale;
    canvas.height = height * pixelScale;
    this.context.scale(pixelScale, pixelScale);
    this.dirty_draw = true;
    this.dirty_viewport = true;
};

App.prototype.send_message = function(msg, data) {
    if (!data.buffer) {
        data = new Uint8Array(data);
    } else if (data.BYTES_PER_ELEMENT != 1) {
        data = new Uint8Array(data.buffer);
    }
    this.worker.postMessage({
        'funcName': msg,
        'data': data
    });
};

App.prototype.tick = function() {

    var pixelScale = window.devicePixelRatio;
    if (pixelScale != this.pixelScale) {
        this.refresh_viewport();
    }

    if (this.dirty_viewport) {

        // If the worker is still busy, don't queue up requests, and don't
        // clear the dirty flag.
        if (!this.pending_collision) {
            this.pending_collision = true;
            this.send_message('d3cpp_set_viewport', this.compute_viewport());
            this.start_time = performance.now();
            this.dirty_viewport = false;
        }

        // Zooming and panning should always be as responsive as possible,
        // even if the worker is bogged down.  So, if the viewport moved,
        // then we definitely need to redraw the canvas.
        this.dirty_draw = true;
    }

    if (this.dirty_draw) {
        this.draw();
        this.dirty_draw = false;
    }

    requestAnimationFrame(this.tick);
};

App.prototype.compute_viewport = function() {
    var xdomain = this.xform.domain();
    var ydomain = this.yform.domain();
    this.viewport[0] = xdomain[0];
    this.viewport[1] = ydomain[0];
    this.viewport[2] = xdomain[1];
    this.viewport[3] = ydomain[1];
    return this.viewport;
};

App.prototype.zoom = function() {
  this.dirty_viewport = true;
};

App.prototype.draw = function() {
    var i = -1, j = 0, data = this.data, n = data.length / 4,
        cx, cy, x0, y0, x1, y1, w, h,
        canvas = this.context, x = this.xform, y = this.yform;
    canvas.clearRect(0, 0, this.winsize[0], this.winsize[1]);
    canvas.strokeStyle = "rgba(0, 0, 0, 0.25)";
    canvas.fillStyle = "rgba(0, 128, 255, 0.1)";

    // Draw black outlines for every non-culled box.
    while (++i < n) {
        x0 = data[j++];
        y0 = data[j++];
        x1 = data[j++];
        y1 = data[j++];
        cx = x(0.5 * (x0 + x1));
        cy = y(0.5 * (y0 + y1));
        w = x1 - x0;
        h = y1 - y0;
        canvas.strokeRect(cx - w * 0.5, cy - h * 0.5, w, h);
    }

    // Draw bluish fill for every box that has overlap.
    if (this.collisions) {
        for (var i = 0; i < this.collisions.length; i += 2) {
            j = this.collisions[i] * 4;
            x0 = data[j++];
            y0 = data[j++];
            x1 = data[j++];
            y1 = data[j];
            cx = x(0.5 * (x0 + x1));
            cy = y(0.5 * (y0 + y1));
            w = x1 - x0;
            h = y1 - y0;
            canvas.fillRect(cx - w * 0.5, cy - h * 0.5, w, h);
            j = this.collisions[i + 1] * 4;
            x0 = data[j++];
            y0 = data[j++];
            x1 = data[j++];
            y1 = data[j];
            cx = x(0.5 * (x0 + x1));
            cy = y(0.5 * (y0 + y1));
            w = x1 - x0;
            h = y1 - y0;
            canvas.fillRect(cx - w * 0.5, cy - h * 0.5, w, h);
        }
    }
};

App.prototype.fetch = function(url, onload) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'arraybuffer';
    var onloadFunc = function() {
        if (xhr.response) {
            onload(xhr.response);
        }
    };
    var errorFunc = function() {
        window.console.error('Unable to download ' + url);
    };
    xhr.onload = onloadFunc;
    xhr.onerror = errorFunc;
    xhr.send();
};

var app = new App();
