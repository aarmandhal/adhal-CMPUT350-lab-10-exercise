#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

#include "open_sans_semibold.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

constexpr int TRUE_MAX_ITERS = 16384;
constexpr int INIT_ITERS = 64;

int main() {
    // OpenGL settings we request for:
    sf::ContextSettings settings;
    settings.depthBits = 24;   // 24-bit depth buffer
    settings.stencilBits = 8;  // 8-bit stencil buffer
    // Request OpenGL 2.1 for compatibility with both SFML/Graphics AND macOS:
    //  - SFML's Graphics module implementation relies on legacy fixed-function pipeline features
    //     of OpenGL, which means only up to OpenGL 2.1. Any newer version requires Compatibility
    //     profile (i.e., maintaining legacy features).
    //  - macOS has deprecated OpenGL support, meaning for anything newer than 2.1 (i.e., 3.0 and
    //      newer), only Core profile is supported, i.e., REMOVING legacy fixed-function features.
    // As a result, if you want to both support macOS AND use SFML's Graphics features (e.g.,
    // sf::RenderWindow), you can use at latest OpenGL 2.1, which is a nightmare.
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    sf::RenderWindow window(sf::VideoMode({1280, 720}), "Shaders", sf::State::Windowed, settings);
    // Query the settings we actually received (may be different):
    const auto& actualSettings = window.getSettings();
    std::cout << "OpenGL version " << actualSettings.majorVersion << "."
              << actualSettings.minorVersion << " flags: " << actualSettings.attributeFlags << '\n';
    // Fullscreen triangle, as described in the prep.
    sf::VertexArray fullscreenTriangle(sf::PrimitiveType::Triangles, 3);
    fullscreenTriangle[0].position = {-1, -1};  // bottom-left
    fullscreenTriangle[1].position = {3, -1};   // bottom-right
    fullscreenTriangle[2].position = {-1, 3};   // top-left
    fullscreenTriangle[0].texCoords = {0, 0};
    fullscreenTriangle[1].texCoords = {2, 0};
    fullscreenTriangle[2].texCoords = {0, 2};
    // load vertex and fragment shaders:
    sf::Shader shader(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"});

    // Second shader + render-texture pair for reprojection
    sf::Shader copyShader(std::filesystem::path{"main.vert"}, std::filesystem::path{"copy.frag"});

    // Double buffered render targets so last frame's image is still readable as lastFrame while
    // this frame is being rendered into the other one.
    sf::RenderTexture renderTextures[2];
    for (auto& rt : renderTextures) {
        if (!rt.resize(window.getSize())) {
            std::cerr << "Failed to create render texture\n";
            return 1;
        }
        rt.setSmooth(true);
    }
    int frontIdx = 0, backIdx = 1;

    sf::Font openSansFont(EmbeddedFonts::OpenSans_SemiBold_ttf,
                          EmbeddedFonts::OpenSans_SemiBold_ttf_len);
    sf::Text cText(openSansFont);
    sf::Text cTextShadow(openSansFont);
    cText.setCharacterSize(14);
    cText.setFillColor(sf::Color::White);
    cText.setPosition({10, 10});
    cTextShadow.setCharacterSize(14);
    cTextShadow.setFillColor(sf::Color(0, 0, 0, 130));
    cTextShadow.setPosition({10, 10});
    cTextShadow.setOutlineColor(sf::Color(0, 0, 0, 130));
    cTextShadow.setOutlineThickness(3);
    sf::Vector2f c{-0.039, 0.68};
    float worldUnitsPerPixel = 1.f / window.getSize().y;
    sf::Vector2f camCenterWorld{0.f, 0.f};
    int maxIters = INIT_ITERS;

    // Previous frame's camera/c state, for reprojecting world coordinates back into last frame's
    // render texture.
    sf::Vector2f oldCamCenterWorld = camCenterWorld;
    float oldWorldUnitsPerPixel = worldUnitsPerPixel;
    sf::Vector2f oldC = c;

    int i = -1;
    while (window.isOpen()) {
        ++i;
        bool moved = false;

        while (auto e = window.pollEvent()) {
            if (e->is<sf::Event::Closed>()) {
                return 0;
            } else if (const auto* keyPressed = e->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scan::Q) {
                    return 0;
                } else if (keyPressed->scancode == sf::Keyboard::Scan::Escape) {
                    return 0;
                }
            } else if (const auto* scrolled = e->getIf<sf::Event::MouseWheelScrolled>()) {
                worldUnitsPerPixel *= std::pow(1.01, scrolled->delta);
                moved = true;
            }
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W)) {
            c.y += 0.001;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S)) {
            c.y -= 0.001;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::A)) {
            c.x -= 0.001;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::D)) {
            c.x += 0.001;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Up)) {
            camCenterWorld.y += 2 * worldUnitsPerPixel;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Down)) {
            camCenterWorld.y -= 2 * worldUnitsPerPixel;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Left)) {
            camCenterWorld.x -= 2 * worldUnitsPerPixel;
            moved = true;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::Right)) {
            camCenterWorld.x += 2 * worldUnitsPerPixel;
            moved = true;
        }
        if (moved) {
            maxIters = INIT_ITERS;
        } else {
            maxIters = std::min(maxIters * 2, TRUE_MAX_ITERS);
        }

        // c only changes via WASD above
        bool cChanged = (i == 0) || (c.x != oldC.x) || (c.y != oldC.y);

        // Set this frame's camera/c state, plus last frame's state and buffer, so the shader can
        // reproject and blend old samples with newly computed ones.
        shader.setUniform("c", sf::Glsl::Vec2(c.x, c.y));
        shader.setUniform("MAX_ITERS", maxIters);
        shader.setUniform("windowSizePixels",
                          sf::Glsl::Vec2(static_cast<float>(window.getSize().x),
                                         static_cast<float>(window.getSize().y)));
        shader.setUniform("camCenterWorld", sf::Glsl::Vec2(camCenterWorld.x, camCenterWorld.y));
        shader.setUniform("camWorldUnitsPerPixel", worldUnitsPerPixel);
        shader.setUniform("oldCamCenterWorld",
                          sf::Glsl::Vec2(oldCamCenterWorld.x, oldCamCenterWorld.y));
        shader.setUniform("oldCamWorldUnitsPerPixel", oldWorldUnitsPerPixel);
        shader.setUniform("cChanged", cChanged);
        shader.setUniform("lastFrame", renderTextures[frontIdx].getTexture());

        // Render this frame into the back buffer rather than straight to the window, so next frame
        // can read it back as "lastFrame" for reprojection.
        renderTextures[backIdx].clear(sf::Color::Black);
        renderTextures[backIdx].draw(fullscreenTriangle, &shader);
        renderTextures[backIdx].display();

        // Blit the back buffer's result onto the actual window.
        copyShader.setUniform("toCopy", renderTextures[backIdx].getTexture());
        window.clear(sf::Color::Black);
        window.draw(fullscreenTriangle, &copyShader);
        cText.setString("c = (" + std::to_string(c.x) + ", " + std::to_string(c.y) + ")");
        cTextShadow.setString("c = (" + std::to_string(c.x) + ", " + std::to_string(c.y) + ")");
        window.draw(cTextShadow);
        window.draw(cText);
        window.display();

        // Advance to next frame
        std::swap(frontIdx, backIdx);
        oldCamCenterWorld = camCenterWorld;
        oldWorldUnitsPerPixel = worldUnitsPerPixel;
        oldC = c;
    }
    return 0;
}