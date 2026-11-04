#version 120

void main() {
    // pass on a 4D vector consisting of our 2D position, at z=0, 
    //  and with canonical homogeneous coords.
    gl_Position = vec4(gl_Vertex.xy, 0, 1); 
    // pass on texCoords as is.
    gl_TexCoord[0] = gl_MultiTexCoord0;
    // pass on color as is.
    gl_FrontColor = gl_Color;
}
