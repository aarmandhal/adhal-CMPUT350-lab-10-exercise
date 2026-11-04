#version 120 

// (way more precision than a float can actually store...)
#define LOG_2 0.6931471805599453094172321214581765680755001

// start using these antialiasing values once you start (c)
#define AA 2
const int TOTAL_SAMPLES = AA * AA;

uniform vec2 c;
uniform int MAX_ITERS;
uniform vec2 windowSizePixels;
uniform vec2 camCenterWorld;
uniform float camWorldUnitsPerPixel;

// Reprojection: last frame's composited render, plus the camera state it was drawn
// with, so a world point can be looked up in last frame's buffer instead of
// recomputed from scratch.
uniform sampler2D lastFrame;
uniform vec2 oldCamCenterWorld;
uniform float oldCamWorldUnitsPerPixel;
uniform bool cChanged;

// Must match the constexpr of the same name on the CPU side
#define TRUE_MAX_ITERS 16384

// Use this to get a color from your smoothed Julia set escape times.
vec4 colorTime(float t) {
    return vec4(0.5 + 0.5 * cos(  0.1 * (t) + vec3(0.f, 0.35, 0.7)), 1.f);
}

struct EscapeTime {
    float iters;
    bool isInf;
};

const float ESCAPE_RADIUS = 4.0;

// Escape time of iterating z^2 + c
EscapeTime escapeTimeSmooth(vec2 z) {
    vec2 zn = z;
    float escapeRadiusSq = ESCAPE_RADIUS * ESCAPE_RADIUS;
    for (int n = 0; n < MAX_ITERS; n++) {
        // Compare squared modulus against squared radius to avoid a sqrt every iteration
        if (dot(zn, zn) > escapeRadiusSq) {
            // Smoothed escape time formula from the prep: n + 1 - ln(ln|z_n|)/ln(2).
            float smoothIters = float(n) + 1.0 - log(log(length(zn))) / LOG_2;
            return EscapeTime(smoothIters, false);
        }
        // z^2 + c, with z^2 expanded for a complex number (x, y): (x^2 - y^2, 2xy).
        zn = vec2(zn.x * zn.x - zn.y * zn.y, 2.0 * zn.x * zn.y) + c;
    }
    
    return EscapeTime(0.0, true);
}


// Pixel (window) coordinates -> world coordinates
vec2 windowToWorld(vec2 windowCoords) {
    return camCenterWorld + (windowCoords - windowSizePixels * 0.5) * camWorldUnitsPerPixel;
}

// For reprojection
// Inverse of windowToWorld
vec2 worldToOldWindow(vec2 worldCoords) {
    return (worldCoords - oldCamCenterWorld) / oldCamWorldUnitsPerPixel + windowSizePixels * 0.5;
}

// For SSAA
// Given (dx, dy) in [0, AA)^2, suppose the unit square [0, 1)^2 was cut up into AA equal
// partitions along each dimension. delta((dx, dy)) returns the center of the sub-square
// which is in the dy'th row and dx'th column (both 0-indexed).
vec2 delta(vec2 dxy) {
    return (dxy + vec2(0.5)) / float(AA);
}


void main() {
    vec2 pixelOrigin = gl_FragCoord.xy - vec2(0.5);
    vec4 colorSum = vec4(0.0);

    for (int dy = 0; dy < AA; dy++) {
        for (int dx = 0; dx < AA; dx++) {
            vec2 sampleWindowCoord = pixelOrigin + delta(vec2(float(dx), float(dy)));
            vec2 sampleWorldCoord  = windowToWorld(sampleWindowCoord);

            // Normal calculation
            EscapeTime et = escapeTimeSmooth(sampleWorldCoord);
            vec4 newColor = et.isInf ? vec4(0.0, 0.0, 0.0, 1.0) : colorTime(et.iters);

            // Where would this same world point have been drawn last frame? 
            // If that falls outside last frame's visible rectangle, there's nothing to reuse.
            vec2 oldWindowCoord = worldToOldWindow(sampleWorldCoord);
            vec2 oldUV = oldWindowCoord / windowSizePixels;
            bool inOldFrame = all(greaterThanEqual(oldUV, vec2(0.0))) &&
                               all(lessThan(oldUV, vec2(1.0)));

            vec4 sampleColor;
            if (cChanged || !inOldFrame) {

                sampleColor = newColor;
            } else {
                // Blend with last frame's value
                vec4 oldColor = texture2D(lastFrame, oldUV);
                float effectiveIters = et.isInf ? float(MAX_ITERS) : et.iters;
                float alpha = clamp(effectiveIters / float(TRUE_MAX_ITERS), 0.0, 1.0);
                sampleColor = mix(oldColor, newColor, alpha);
            }

            colorSum += sampleColor;
        }
    }

    // Average all AA*AA sub-pixel samples into this pixel's final color.
    gl_FragColor = colorSum / float(TOTAL_SAMPLES);
}