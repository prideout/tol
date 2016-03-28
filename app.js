'using strict';

var App = function() {

    this.worker = new Worker('worker.js');
    this.circles = null;
    this.start_time = performance.now();
    this.outer_radius = 200;
    this.winsize = new Float32Array(2);
    this.viewport = new Float32Array(4);

    // This flag enables us to avoid queuing up work when culling takes
    // longer than a single frame.
    this.pending = false;

    this.worker.onmessage = function(msg) {
        if (msg.data.event == 'bubbles') {
            this.on_bubbles(msg.data.bubbles);
        }
    }.bind(this);

    var canvas = document.getElementsByTagName('canvas')[0];
    var width = this.winsize[0] = canvas.clientWidth;
    var height = this.winsize[1] = canvas.clientHeight;
    this.send_message('d3cpp_set_winsize', this.winsize);

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

    var url = 'http://broadphase.net/monolith.0000.partial.txt';
    this.fetch(url, function(arraybuf) {
        this.send_message('d3cpp_set_monolith', arraybuf)
    }.bind(this));

    this.tick = this.tick.bind(this);
    this.tick();
};

App.prototype.on_bubbles = function(bubbles) {
    this.pending = false;
    this.circles = new Float32Array(bubbles.buffer);
    this.dirty_draw = true;
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
        if (!this.pending) {
            this.pending = true;
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
    var i, cx, cy, r, w, h,
        ctx = this.context, x = this.xform, y = this.yform,
        twopi = 2 * Math.PI,
        hx = this.winsize[0] * 0.5,
        hy = this.winsize[1] * 0.5,
        xyscale = this.outer_radius,
        rscale = xyscale * (x.range()[1] / (x.domain()[1] - x.domain()[0]));

    ctx.clearRect(0, 0, this.winsize[0], this.winsize[1]);
    ctx.strokeStyle = "rgba(0, 0, 0, 0.4)";
    ctx.fillStyle = "rgba(0, 128, 255, 0.1)";

    if (this.circles) {
        for (i = 0; i < this.circles.length;) {
            cx = x(xyscale * this.circles[i++] + hx);
            cy = y(xyscale * this.circles[i++] + hy);
            r = rscale * this.circles[i++];
            ctx.beginPath();
            ctx.arc(cx, cy, r, 0, twopi);
            ctx.fill();
            ctx.stroke();
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
