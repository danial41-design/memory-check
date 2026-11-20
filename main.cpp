#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


struct Color { float r, g, b, a; };
struct Vertex { float x, y; Color color; };
struct KeyItem { float x, y; bool collected; };
enum GameState { Ready, Memorize, Playing, Won };

const int MAP_WIDTH=24, MAP_HEIGHT=14;
const float CANVAS_WIDTH=1120.0f, CANVAS_HEIGHT=800.0f;
const float TILE_SIZE=40.0f, BOARD_X=80.0f, BOARD_Y=136.0f;
const float PLAYER_HALF_SIZE=11.0f, PLAYER_SPEED=150.0f;
const float MEMORIZE_SECONDS=6.0f, FLASH_SECONDS=0.5f;
const int MAX_FLASHES=2;


const char* roomRows[MAP_HEIGHT]={
    "########################",
    "#......................#",
    "#..1...#..###........E.#",
    "#......#..###..#.......#",
    "#......#.......#.......#",
    "#......#....2..#..###..#",
    "#..............#..###..#",
    "#......#.......#.......#",
    "#..###.#...............#",
    "#..###.#.......#.......#",
    "#......#.......#....3..#",
    "#.S............#.......#",
    "#......................#",
    "########################"
};

GameState gameState=Ready;
std::vector<KeyItem> keys;
std::vector<Vertex> drawVertices;
float playerX=0, playerY=0, spawnX=0, spawnY=0, exitX=0, exitY=0;
float facingX=1, facingY=0;
float memorizeTime=MEMORIZE_SECONDS, gameTime=0, flashTime=0;
float messageTime=0, impactTime=0;
int collectedKeys=0, flashesRemaining=MAX_FLASHES, collisions=0;
bool wasBlocked=false;
std::string message;
int framebufferWidth=1120, framebufferHeight=800;
unsigned int shaderProgram=0, vao=0, vbo=0;
int canvasLocation=-1;

const Color textColor={0.87f,0.91f,0.93f,1};
const Color mutedColor={0.48f,0.58f,0.64f,1};
const Color cyanColor={0.23f,0.84f,0.87f,1};
const Color goldColor={1.0f,0.76f,0.30f,1};
const Color greenColor={0.40f,0.88f,0.60f,1};

bool loadLevel()
{
    keys.clear();
    int spawnCount=0, exitCount=0;
    for (int row=0; row<MAP_HEIGHT; ++row)
    {
        if (roomRows[row]==nullptr)
        {
            std::cerr << "Missing map row " << row+1 << ".\n"; return false;
        }
        if (std::strlen(roomRows[row])!=MAP_WIDTH)
        {
            std::cerr << "Invalid map row width at row " << row+1
                      << ": expected " << MAP_WIDTH << " cells.\n"; return false;
        }
        for (int col=0; col<MAP_WIDTH; ++col)
        {
            char cell=roomRows[row][col];
            float x=BOARD_X+(static_cast<float>(col)+0.5f)*TILE_SIZE;
            float y=BOARD_Y+(static_cast<float>(row)+0.5f)*TILE_SIZE;
            if (cell=='S') { spawnX=x; spawnY=y; ++spawnCount; }
            if (cell=='E') { exitX=x; exitY=y; ++exitCount; }
            if (cell>='1' && cell<='3') keys.push_back({x,y,false});
            if ((row==0 || row==MAP_HEIGHT-1 || col==0 || col==MAP_WIDTH-1) && cell!='#')
            {
                std::cerr << "Map must have closed outer walls.\n"; return false;
            }
        }
    }
    if (spawnCount!=1 || exitCount!=1 || keys.size()!=3)
    {
        std::cerr << "Map needs one start, one exit and three keys.\n"; return false;
    }
    return true;
}

void setMessage(const std::string& text, float duration)
{
    message=text; messageTime=duration;
}
void startGame()
{
    gameState=Memorize;
    playerX=spawnX; playerY=spawnY; facingX=1; facingY=0;
    memorizeTime=MEMORIZE_SECONDS; gameTime=0; flashTime=0;
    messageTime=0; impactTime=0; collectedKeys=0;
    flashesRemaining=MAX_FLASHES; collisions=0; wasBlocked=false;
    for (KeyItem& key : keys) key.collected=false;
}
void useFlash()
{
    if (gameState!=Playing || flashTime>0) return;
    if (flashesRemaining<=0) { setMessage("NO FLASHES LEFT",1.2f); return; }
    --flashesRemaining; flashTime=FLASH_SECONDS;
    setMessage("FLASH - CHECK YOUR MEMORY",FLASH_SECONDS);
}
bool isRoomVisible()
{
    return gameState==Memorize || gameState==Won || (gameState==Playing && flashTime>0);
}

bool checkCollisions(float x, float y)
{
    int left=static_cast<int>(std::floor((x-PLAYER_HALF_SIZE-BOARD_X)/TILE_SIZE));
    int right=static_cast<int>(std::floor((x+PLAYER_HALF_SIZE-0.001f-BOARD_X)/TILE_SIZE));
    int top=static_cast<int>(std::floor((y-PLAYER_HALF_SIZE-BOARD_Y)/TILE_SIZE));
    int bottom=static_cast<int>(std::floor((y+PLAYER_HALF_SIZE-0.001f-BOARD_Y)/TILE_SIZE));
    if (left<0 || right>=MAP_WIDTH || top<0 || bottom>=MAP_HEIGHT) return true;
    for (int row=top; row<=bottom; ++row)
        for (int col=left; col<=right; ++col)
            if (roomRows[row][col]=='#') return true;
    return false;
}
void updatePlayer(float directionX, float directionY, float dt)
{
    if (gameState!=Playing) return;
    float length=std::sqrt(directionX*directionX+directionY*directionY);
    if (length<=0) { wasBlocked=false; return; }
    directionX/=length; directionY/=length;
    facingX=directionX; facingY=directionY;
    float dx=directionX*PLAYER_SPEED*dt, dy=directionY*PLAYER_SPEED*dt;
    int steps=std::max(1,static_cast<int>(std::ceil(std::max(std::fabs(dx),std::fabs(dy))/8.0f)));
    dx/=static_cast<float>(steps); dy/=static_cast<float>(steps);
    bool blocked=false;
    for (int step=0; step<steps; ++step)
    {
        if (dx!=0)
        {
            if (!checkCollisions(playerX+dx,playerY)) playerX+=dx;
            else blocked=true;
        }
        if (dy!=0)
        {
            if (!checkCollisions(playerX,playerY+dy)) playerY+=dy;
            else blocked=true;
        }
    }
    if (blocked && !wasBlocked)
    {
        ++collisions; impactTime=0.18f;
        setMessage("WALL - TRY ANOTHER DIRECTION",0.8f);
    }
    wasBlocked=blocked;
}
void processInput(GLFWwindow* window, float dt)
{
    float dx=0, dy=0;
    if (glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS || glfwGetKey(window,GLFW_KEY_LEFT)==GLFW_PRESS) dx-=1;
    if (glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS || glfwGetKey(window,GLFW_KEY_RIGHT)==GLFW_PRESS) dx+=1;
    if (glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS || glfwGetKey(window,GLFW_KEY_UP)==GLFW_PRESS) dy-=1;
    if (glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS || glfwGetKey(window,GLFW_KEY_DOWN)==GLFW_PRESS) dy+=1;
    updatePlayer(dx,dy,dt);
}
void collectKeys()
{
    for (KeyItem& key : keys)
    {
        float dx=playerX-key.x, dy=playerY-key.y;
        if (!key.collected && dx*dx+dy*dy<18.0f*18.0f)
        {
            key.collected=true; ++collectedKeys;
            if (collectedKeys==static_cast<int>(keys.size()))
                setMessage("ALL KEYS FOUND - EXIT UNLOCKED",3.0f);
            else setMessage("KEY FOUND - KEEP GOING",1.5f);
        }
    }
}
void updateGame(float dt)
{
    messageTime=std::max(0.0f,messageTime-dt);
    impactTime=std::max(0.0f,impactTime-dt);
    if (gameState==Memorize)
    {
        memorizeTime=std::max(0.0f,memorizeTime-dt);
        if (memorizeTime<=0)
        {
            gameState=Playing;
            setMessage("LIGHTS OUT - TRUST YOUR MEMORY",2.0f);
        }
    }
    else if (gameState==Playing)
    {
        gameTime+=dt;
        flashTime=std::max(0.0f,flashTime-dt);
        collectKeys();
        if (collectedKeys==static_cast<int>(keys.size()) &&
            std::fabs(playerX-exitX)<16 && std::fabs(playerY-exitY)<16)
        {
            gameState=Won; flashTime=0; messageTime=0;
        }
    }
}

void addRectangle(float x, float y, float width, float height, Color color)
{
    drawVertices.insert(drawVertices.end(),{
        {x,y,color},{x+width,y,color},{x+width,y+height,color},
        {x,y,color},{x+width,y+height,color},{x,y+height,color}
    });
}

const char* fontCharacters="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:.-/!?";
const unsigned char fontRows[][7]={
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
    {0,4,4,0,4,4,0}, {0,0,0,0,0,12,12},
    {0,0,0,31,0,0,0}, {1,2,2,4,8,8,16},
    {4,4,4,4,4,0,4}, {14,17,1,2,4,0,4}
};
void drawText(const std::string& text, float x, float y, float scale, Color color)
{
    for (char letter : text)
    {
        const char* found=std::strchr(fontCharacters,letter);
        if (found)
        {
            size_t index=static_cast<size_t>(found-fontCharacters);
            for (int row=0; row<7; ++row)
                for (int col=0; col<5; ++col)
                    if ((fontRows[index][row] & (1 << (4-col)))!=0)
                        addRectangle(x+static_cast<float>(col)*scale,y+static_cast<float>(row)*scale,scale,scale,color);
        }
        x+=6.0f*scale;
    }
}
void drawCenteredText(const std::string& text, float y, float scale, Color color)
{
    float width=(static_cast<float>(text.size())*6.0f-1.0f)*scale;
    drawText(text,(CANVAS_WIDTH-width)*0.5f,y,scale,color);
}
std::string formatTime(float seconds)
{
    std::ostringstream result;
    result << std::fixed << std::setprecision(1) << seconds;
    return result.str();
}
void drawKey(float x, float y)
{
    addRectangle(x-12,y-5,9,3,goldColor);
    addRectangle(x-12,y+2,9,3,goldColor);
    addRectangle(x-12,y-5,3,10,goldColor);
    addRectangle(x-6,y-5,3,10,goldColor);
    addRectangle(x-3,y-1,15,3,goldColor);
    addRectangle(x+5,y+1,3,6,goldColor);
    addRectangle(x+10,y+1,3,6,goldColor);
}
void drawRoom()
{
    addRectangle(BOARD_X-3,BOARD_Y-3,MAP_WIDTH*TILE_SIZE+6,MAP_HEIGHT*TILE_SIZE+6,{0.18f,0.25f,0.29f,1});
    addRectangle(BOARD_X,BOARD_Y,MAP_WIDTH*TILE_SIZE,MAP_HEIGHT*TILE_SIZE,{0.015f,0.022f,0.030f,1});
    if (!isRoomVisible()) return;
    for (int row=0; row<MAP_HEIGHT; ++row)
        for (int col=0; col<MAP_WIDTH; ++col)
        {
            float x=BOARD_X+static_cast<float>(col)*TILE_SIZE;
            float y=BOARD_Y+static_cast<float>(row)*TILE_SIZE;
            if (roomRows[row][col]=='#')
            {
                addRectangle(x,y,TILE_SIZE,TILE_SIZE,{0.28f,0.36f,0.40f,1});
                addRectangle(x+2,y+2,TILE_SIZE-4,3,{0.40f,0.49f,0.52f,1});
                addRectangle(x+2,y+TILE_SIZE-5,TILE_SIZE-4,3,{0.17f,0.24f,0.28f,1});
            }
            else
            {
                float shade=(row+col)%2==0 ? 0.105f : 0.12f;
                addRectangle(x,y,TILE_SIZE,TILE_SIZE,{shade,shade+0.035f,shade+0.055f,1});
            }
        }
    bool unlocked=collectedKeys==static_cast<int>(keys.size());
    Color doorColor=unlocked ? greenColor : Color{0.25f,0.48f,0.36f,1};
    addRectangle(exitX-16,exitY-18,32,36,doorColor);
    addRectangle(exitX-12,exitY-14,24,28,{0.07f,0.15f,0.13f,1});
    drawText("E",exitX-5,exitY-7,2,doorColor);
    for (const KeyItem& key : keys)
        if (!key.collected) drawKey(key.x,key.y);
}
void drawPlayer()
{
    if (gameState==Ready) return;
    Color color=impactTime>0 ? Color{1.0f,0.40f,0.36f,1} : cyanColor;
    addRectangle(playerX-PLAYER_HALF_SIZE-2,playerY-PLAYER_HALF_SIZE-2,26,26,{0.06f,0.18f,0.22f,1});
    addRectangle(playerX-PLAYER_HALF_SIZE,playerY-PLAYER_HALF_SIZE,22,22,color);
    addRectangle(playerX-6,playerY-6,12,12,{0.12f,0.40f,0.45f,1});
    addRectangle(playerX+facingX*7-2,playerY+facingY*7-2,4,4,textColor);
}
void drawHUD()
{
    drawText("MEMORY ROOM",56,23,3.0f,textColor);
    drawText("LOOK. REMEMBER. FIND THE WAY.",56,54,1.6f,mutedColor);
    std::string phase="READY";
    Color phaseColor=cyanColor;
    if (gameState==Memorize) { phase="MEMORIZE "+formatTime(memorizeTime)+"S"; phaseColor=goldColor; }
    if (gameState==Playing) { phase=flashTime>0 ? "FLASH" : "LIGHTS OUT"; phaseColor=flashTime>0 ? goldColor : cyanColor; }
    if (gameState==Won) { phase="ROOM COMPLETE"; phaseColor=greenColor; }
    drawText(phase,715,25,2.4f,phaseColor);
    if (gameState==Memorize)
    {
        addRectangle(715,59,320,5,{0.18f,0.23f,0.27f,1});
        addRectangle(715,59,320*memorizeTime/MEMORIZE_SECONDS,5,goldColor);
    }
    addRectangle(80,87,960,31,{0.09f,0.13f,0.17f,1});
    drawText("KEYS "+std::to_string(collectedKeys)+"/3",94,96,1.7f,goldColor);
    drawText("FLASHES "+std::to_string(flashesRemaining),310,96,1.7f,cyanColor);
    drawText("TIME "+formatTime(gameTime)+"S",570,96,1.7f,textColor);
    drawText("HITS "+std::to_string(collisions),845,96,1.7f,textColor);
    drawText("WASD / ARROWS MOVE    SPACE FLASH    R RESTART    ESC QUIT",80,719,1.8f,mutedColor);
    std::string hint="PRESS ENTER TO START";
    if (gameState==Memorize) hint="REMEMBER THE WALLS, THREE KEYS AND THE EXIT";
    if (gameState==Playing) hint=collectedKeys==3 ? "EXIT UNLOCKED - FIND THE DOOR" : "FIND THREE KEYS, THEN REACH THE EXIT";
    if (gameState==Won) hint="PRESS ENTER OR R TO TRY AGAIN";
    if (messageTime>0) hint=message;
    drawText(hint,80,755,1.9f,gameState==Memorize ? goldColor : textColor);
}
void drawOverlay()
{
    if (gameState!=Ready && gameState!=Won) return;
    addRectangle(230,283,660,263,{0.08f,0.12f,0.16f,0.97f});
    addRectangle(230,283,660,4,gameState==Won ? greenColor : cyanColor);
    if (gameState==Ready)
    {
        drawCenteredText("CAN YOU REMEMBER THE ROOM?",312,2.5f,textColor);
        drawCenteredText("6 SECONDS TO MEMORIZE. THEN LIGHTS OUT.",363,1.8f,mutedColor);
        drawCenteredText("FIND 3 KEYS. REMEMBER THE EXIT.",396,1.8f,goldColor);
        drawCenteredText("ONLY 2 SHORT FLASHES OF LIGHT.",426,1.8f,mutedColor);
        drawCenteredText("PRESS ENTER",484,2.6f,cyanColor);
    }
    else
    {
        drawCenteredText("ROOM COMPLETE!",313,3.5f,greenColor);
        drawCenteredText("TIME "+formatTime(gameTime)+"S",373,2.5f,textColor);
        drawCenteredText("COLLISIONS "+std::to_string(collisions)+"    FLASHES USED "+std::to_string(MAX_FLASHES-flashesRemaining),419,1.9f,goldColor);
        drawCenteredText("ENTER / R - PLAY AGAIN",488,2.1f,cyanColor);
    }
}

const char* vertexShaderSource=R"(
#version 330 core
layout (location=0) in vec2 aPosition;
layout (location=1) in vec4 aColor;
uniform vec2 uCanvasSize;
out vec4 vertexColor;
void main()
{
    vec2 clip=vec2(2.0*aPosition.x/uCanvasSize.x-1.0,
                   1.0-2.0*aPosition.y/uCanvasSize.y);
    gl_Position=vec4(clip,0.0,1.0);
    vertexColor=aColor;
}
)";
const char* fragmentShaderSource=R"(
#version 330 core
in vec4 vertexColor;
out vec4 FragColor;
void main() { FragColor=vertexColor; }
)";
unsigned int compileShader(GLenum type, const char* source)
{
    unsigned int shader=glCreateShader(type);
    glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
    int success=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
    if (!success)
    {
        char log[4096]; glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        std::cerr << "Shader compilation error:\n" << log << '\n';
        glDeleteShader(shader); return 0;
    }
    return shader;
}
bool createRenderer()
{
    unsigned int vs=compileShader(GL_VERTEX_SHADER,vertexShaderSource);
    unsigned int fs=compileShader(GL_FRAGMENT_SHADER,fragmentShaderSource);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return false; }
    shaderProgram=glCreateProgram();
    glAttachShader(shaderProgram,vs); glAttachShader(shaderProgram,fs);
    glLinkProgram(shaderProgram); glDeleteShader(vs); glDeleteShader(fs);
    int success=0; glGetProgramiv(shaderProgram,GL_LINK_STATUS,&success);
    if (!success)
    {
        char log[4096]; glGetProgramInfoLog(shaderProgram,sizeof(log),nullptr,log);
        std::cerr << "Shader link error:\n" << log << '\n'; return false;
    }
    canvasLocation=glGetUniformLocation(shaderProgram,"uCanvasSize");
    glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
    glEnableVertexAttribArray(1); glBindVertexArray(0);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    drawVertices.reserve(40000);
    return glGetError()==GL_NO_ERROR;
}
void renderScene()
{
    if (framebufferWidth<=0 || framebufferHeight<=0) return;
    glClearColor(0.035f,0.052f,0.070f,1); glClear(GL_COLOR_BUFFER_BIT);
    drawVertices.clear();
    drawRoom(); drawPlayer(); drawHUD(); drawOverlay();
    glUseProgram(shaderProgram);
    glUniform2f(canvasLocation,CANVAS_WIDTH,CANVAS_HEIGHT);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,drawVertices.size()*sizeof(Vertex),drawVertices.data(),GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES,0,static_cast<int>(drawVertices.size()));
    glBindVertexArray(0);
}
void resizeCallback(GLFWwindow*, int width, int height)
{
    framebufferWidth=width; framebufferHeight=height;
    if (width<=0 || height<=0) return;
    float scale=std::min(static_cast<float>(width)/CANVAS_WIDTH,static_cast<float>(height)/CANVAS_HEIGHT);
    int viewWidth=std::max(1,static_cast<int>(CANVAS_WIDTH*scale));
    int viewHeight=std::max(1,static_cast<int>(CANVAS_HEIGHT*scale));
    glViewport((width-viewWidth)/2,(height-viewHeight)/2,viewWidth,viewHeight);
}
void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (action!=GLFW_PRESS) return;
    if (key==GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window,true);
    if (key==GLFW_KEY_ENTER && (gameState==Ready || gameState==Won)) startGame();
    if (key==GLFW_KEY_R) startGame();
    if (key==GLFW_KEY_SPACE) useFlash();
}
void errorCallback(int, const char* description)
{
    std::cerr << "GLFW: " << description << '\n';
}
void deleteRenderer()
{
    glDeleteBuffers(1,&vbo); glDeleteVertexArrays(1,&vao); glDeleteProgram(shaderProgram);
}
int main()
{
    if (!loadLevel()) return 1;
    playerX=spawnX; playerY=spawnY;
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window=glfwCreateWindow(1120,800,"Memory Room - Lights-Out Memory Puzzle",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cerr << "Cannot initialize GLAD.\n";
        glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    if (!createRenderer())
    {
        deleteRenderer(); glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
    glfwSwapInterval(1);
    glfwSetKeyCallback(window,keyCallback);
    glfwSetFramebufferSizeCallback(window,resizeCallback);
    glfwSetWindowSizeLimits(window,700,500,GLFW_DONT_CARE,GLFW_DONT_CARE);
    glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);
    resizeCallback(window,framebufferWidth,framebufferHeight);
    std::cout << "Memory Room - Daniyal Amangeldi, group 2412\n"
              << "OpenGL: " << glGetString(GL_VERSION) << '\n'
              << "Enter: start | WASD/arrows: move | Space: flash | R: restart | Esc: quit\n"
              << "Remember the room for 6 seconds. Find three keys and the exit in darkness.\n"
              << std::flush;
    double previousTime=glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        double now=glfwGetTime();
        float dt=static_cast<float>(now-previousTime);
        previousTime=now;
        processInput(window,std::min(dt,0.05f)); updateGame(dt); renderScene();
        glfwSwapBuffers(window);
        if (framebufferWidth<=0 || framebufferHeight<=0) glfwWaitEventsTimeout(0.05);
    }
    deleteRenderer(); glfwDestroyWindow(window); glfwTerminate();
    return 0;
}
