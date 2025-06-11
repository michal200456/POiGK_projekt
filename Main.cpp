/*

Sterowanie:
= zwiększ nastawę złącza
- zmniejsz nastawę złącza
PageUp wybierz kolejne złącze
PageDown wybierz poprzednie złącze
Enter edytuj wartość nastawy złącza
MMB przełącz sterowanie kamerą
W ruch kamery w przód
A ruch kamery w lewo
S ruch kamery w tył
D ruch kamery w prawo
E ruch kamery do góry
Q ruch kamery w dół
U przełącz w tryb uczenia
Ctrl+S zapisz pozycję robota
Delete usuń ostatnią zapisaną pozycję robota
P przełącz w tryb pracy

*/

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <memory>
#include <vector>
#include <cstring>
#define RAYGUI_IMPLEMENTATION
#include "external/raylib/raygui.h"

enum JointType {
    // rodzaj złącza
    REVOLUTE,
    PRISMATIC,
    MANIPULATOR
};

Matrix MatrixTranslate(Vector3 translation) {
    return MatrixTranslate(translation.x, translation.y, translation.z);
}

Matrix DHtoMatrix(Vector4 DH) {
    // Przekształca parametry Denavit-Hartenberga (DH) 
    Matrix result;
    Matrix RT = MatrixMultiply(MatrixRotateX(DH.x), MatrixTranslate(DH.y, 0, 0));
    Matrix TR = MatrixMultiply(MatrixTranslate(0, DH.z, 0), MatrixRotateY(DH.w));
    result = MatrixMultiply(RT, TR);
    return result;
}

class clCamera {
    // Symuluje kamerę 3D typu FPS
public:
    clCamera(Vector3 pos) {
        parameters.position = pos;
        parameters.target = Vector3Add(pos, {0, 0, -1});
        parameters.up = {0, 1, 0};
        parameters.fovy = 45.0f;
        parameters.projection = CAMERA_PERSPECTIVE;

        SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
    }

    void Update() {
        // aktualizuje kąt kamery przy ruszaniu myszką
        Vector2 mouseDelta = GetMouseDelta();
        yaw -= mouseDelta.x * 0.002f;
        pitch -= mouseDelta.y * 0.002f;

        pitch = Clamp(pitch, -PI / 2.0f + 0.01f, PI / 2.0f - 0.01f);

        Vector3 forward = {
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        };

        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, parameters.up));

        float speed = 0.1f;
        // ruch kamery w każdym kierunku
        if (IsKeyDown(KEY_W)) parameters.position = Vector3Add(parameters.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_S)) parameters.position = Vector3Subtract(parameters.position, Vector3Scale(forward, speed));
        if (IsKeyDown(KEY_A)) parameters.position = Vector3Subtract(parameters.position, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_D)) parameters.position = Vector3Add(parameters.position, Vector3Scale(right, speed));
        if (IsKeyDown(KEY_E)) parameters.position.y += speed;
        if (IsKeyDown(KEY_Q)) parameters.position.y -= speed;

        parameters.target = Vector3Add(parameters.position, forward);
    }

    Camera3D parameters;

private:
    float yaw = 0.0f;
    float pitch = 0.0f;
};

// shadery symulujące oświetlenie robota z jednej strony
const char* vertexShaderCode = R"(
    #version 330
    uniform mat4 mvp;
    uniform mat4 matModel;
    in vec3 vertexPosition;
    in vec3 vertexNormal;

    out vec3 fragNormal;
    out vec3 fragPos;

    void main()
    {
        fragPos = vec3(matModel * vec4(vertexPosition, 1.0));
        fragNormal = mat3(transpose(inverse(matModel))) * vertexNormal;
        gl_Position = mvp * vec4(vertexPosition, 1.0);
    }
    )";

const char* fragmentShaderCode = R"(
    #version 330
    in vec3 fragNormal;
    in vec3 fragPos;

    uniform vec3 lightDir; // should be normalized
    uniform vec4 baseColor;

    out vec4 finalColor;

    void main()
    {
        vec3 norm = normalize(fragNormal);
        float diff = max(dot(norm, -lightDir), 0.0);
        vec3 diffuse = diff * baseColor.rgb;
        vec3 ambient = 0.2 * baseColor.rgb;
        finalColor = vec4(diffuse + ambient, baseColor.a);
    }
    )";

class Device {
public:
    Device(const char* fileName, Shader& shaderRef) : shader(shaderRef) {
        model = LoadModel(fileName);
        absoluteTransforms.resize(model.boneCount);
        DHparameters.resize(model.boneCount); 
        // nadanie parametrów DH
        absoluteTransforms[0] = MatrixTranslate(model.bindPose[0].translation);
        DHparameters[0] = {0, 0, 0, 0};
        for (int i = 1; i < model.boneCount; i++) {
            absoluteTransforms[i] = MatrixTranslate(model.bindPose[i].translation);
            DHparameters[i] = { 0, model.bindPose[i].translation.x - model.bindPose[0].translation.x, model.bindPose[i].translation.y - model.bindPose[0].translation.y, 0};
        }
        offset = DHparameters[2].y + DHparameters[1].y;

        for (int i = 0; i < model.materialCount; i++) {
            model.materials[i].shader = shader;
        }
    }

    ~Device() {
        UnloadModel(model);
    }

    void Draw(Color clr, const Camera3D& cam, Shader& shader) {
         // rysowanie i dodawanie światła
        Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
        Matrix projection = MatrixPerspective(cam.fovy * DEG2RAD, (float)GetScreenWidth() / GetScreenHeight(), 0.01f, 1000.0f);

        for (int i = 0; i < model.meshCount; i++) {
            Matrix mModel = absoluteTransforms[i];
            Matrix mvp = MatrixMultiply(MatrixMultiply(mModel, view), projection);
            // shadery
            SetShaderValue(shader, GetShaderLocation(shader, "mvp"), &mvp, 4);
            SetShaderValue(shader, GetShaderLocation(shader, "matModel"), &mModel, 4);

            Vector3 lightDir = Vector3Normalize({-0.5f, -1.0f, -0.3f});
            SetShaderValue(shader, GetShaderLocation(shader, "lightDir"), &lightDir, SHADER_UNIFORM_VEC3);

            Vector4 baseColor = { clr.r / 255.0f, clr.g / 255.0f, clr.b / 255.0f, clr.a / 255.0f };
            SetShaderValue(shader, GetShaderLocation(shader, "baseColor"), &baseColor, SHADER_UNIFORM_VEC4);

            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], mModel);
        }
    }

    void MoveJoint(float newValue) {
         // zmienia rozstaw chwytaka
        DHparameters[2].y = (offset - newValue) / 2.f;
        DHparameters[1].y = (offset + newValue) / 2.f;
    }

    float GetPosition() {
        return DHparameters[1].y - DHparameters[2].y;
    }

    void UpdateTransforms(Matrix origin) {
        absoluteTransforms[0] = origin;
        for (int i = 1; i < model.boneCount; i++) {
            absoluteTransforms[i] = MatrixMultiply(DHtoMatrix(DHparameters[i]), absoluteTransforms[0]);
        }
    }

    Model model;
    std::vector<Matrix> absoluteTransforms;
    std::vector<Vector4> DHparameters;
    float offset;
    Shader& shader;
};

class RobotArm {
public:
    RobotArm(const char* fileName, Shader& shaderRef) : shader(shaderRef) {
        model = LoadModel(fileName);  // wczytywanie modelu 
        absoluteTransforms.resize(model.boneCount);
        DHparameters.resize(model.boneCount);
        jointTypes.resize(model.boneCount);
        absoluteTransforms[0] = MatrixTranslate(model.bindPose[0].translation);
        DHparameters[0] = { 0, 0, 0, 0 };
        // nadanie parametrów DH
        for (int i = 1; i < model.boneCount; i++) {
            absoluteTransforms[i] = MatrixTranslate(model.bindPose[i].translation);
            DHparameters[i] = { 0, 0, model.bindPose[i].translation.y - model.bindPose[i - 1].translation.y, 0 };
            jointTypes[i] = REVOLUTE;
        }
        jointTypes[model.boneCount - 1] = MANIPULATOR;

        for (int i = 0; i < model.materialCount; i++) {
            model.materials[i].shader = shader;
        }

        targetPositions.resize(model.boneCount);
        for (int i = 0; i < model.boneCount; i++) {
            targetPositions[i] = GetJointPosition(i);
        }
    }

    ~RobotArm() {
        UnloadModel(model);
    }

    void LoadDevice(const char* fileName) {
        device.reset();
        device = std::make_unique<Device>(fileName, shader);
        device->UpdateTransforms(absoluteTransforms[model.boneCount - 1]);
        targetPositions[model.boneCount - 1] = device->GetPosition();
    }

    void Draw(int selection, const Camera3D& cam, Shader& shader) {
        // rysowanie robota wraz z shaderami
        Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
        Matrix projection = MatrixPerspective(cam.fovy * DEG2RAD, (float)GetScreenWidth() / GetScreenHeight(), 0.01f, 1000.0f);

        for (int i = 0; i < model.meshCount; i++) {
            Matrix mModel = absoluteTransforms[i];
            Matrix mvp = MatrixMultiply(MatrixMultiply(mModel, view), projection);
              // shadery
            SetShaderValue(shader, GetShaderLocation(shader, "mvp"), &mvp, 4);
            SetShaderValue(shader, GetShaderLocation(shader, "matModel"), &mModel, 4);

            Vector3 lightDir = Vector3Normalize({-0.5f, -1.0f, -0.3f});
            SetShaderValue(shader, GetShaderLocation(shader, "lightDir"), &lightDir, SHADER_UNIFORM_VEC3);

            Color clr = (i == selection) ? YELLOW : WHITE; //zaznaczenie kolorem wybranego przegubu
            Vector4 baseColor = { clr.r / 255.0f, clr.g / 255.0f, clr.b / 255.0f, clr.a / 255.0f };
            SetShaderValue(shader, GetShaderLocation(shader, "baseColor"), &baseColor, SHADER_UNIFORM_VEC4);

            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], mModel);
        }
        if (device) {
            Color clr = (model.meshCount == selection) ? YELLOW : WHITE;
            device->Draw(clr, cam, shader);
        }
    }

    void MoveJoint(int selection, float newValue) {
        // aktualizacja pozycji przegubów
        switch (jointTypes[selection]) {
        case REVOLUTE:
            DHparameters[selection].x = newValue * DEG2RAD;
            break;
        case PRISMATIC:
            DHparameters[selection].y = newValue;
            break;
        case MANIPULATOR:
            if (device) device->MoveJoint(newValue);
            break;
        }

        for (int i = 1; i < model.boneCount; i++) {
            absoluteTransforms[i] = MatrixMultiply(DHtoMatrix(DHparameters[i]), absoluteTransforms[i - 1]);
        }
        device->UpdateTransforms(absoluteTransforms[model.boneCount - 1]);
    }

    float GetJointPosition(int selection) {
        switch (jointTypes[selection]) {
        case REVOLUTE:
            return RAD2DEG * DHparameters[selection].x;
        case PRISMATIC:
            return DHparameters[selection].y;
        case MANIPULATOR:
            if (device) return device->GetPosition();
        }
        return 0;
    }

    void MoveJointDiscrete(int selection, int direction) {
         // przesuwanie przegubu
        float delta;
        switch (jointTypes[selection]) {
        case REVOLUTE:
            delta = 5;
            break;
        case PRISMATIC:
            delta = 0.1;
            break;
        case MANIPULATOR:
            delta = 0.05;
            break;
        }
        UpdateTargetPosition(selection, GetJointPosition(selection) + direction * delta);
    }

    bool UpdateJointsSmooth(float lerpFactor = 0.1f) {
        // animacja przejścia złącza do nadanej pozycji
        bool jointMoves = false;
        for (int i = 0; i < (int)targetPositions.size(); i++) {
            float currentPos = GetJointPosition(i);
            float diff = targetPositions[i] - currentPos;
            if (fabs(diff) > 0.001f) {
                float newPos = currentPos + diff * lerpFactor;
                MoveJoint(i, newPos);
                jointMoves = true;
            }
        }
        return jointMoves;
    }

    void UpdateTargetPosition(int selection, float newValue) {
        targetPositions[selection] = newValue;
    }

    std::unique_ptr<Device> device;
    Model model;
    std::vector<Matrix> absoluteTransforms;
    std::vector<Vector4> DHparameters;
    std::vector<JointType> jointTypes;
    std::vector<float> targetPositions;
    Shader& shader;
};

//zapisane pozycje robota w trybie nauki
class SavedStates {
public:
    //zapisanie pozycji robota
    void Save(RobotArm& robot) {
        for (int i = 1;i < jointCount + 1;i++) {
            c.push_back(robot.GetJointPosition(i));
        }
        statesCount++;
    }
    //usunięcie ostatniej pozycji robota
    void Delete() {
        if (statesCount == 0) return;
        for (int i = 0;i < jointCount;i++) {
            c.pop_back();
        }
        statesCount--;
    }
    void GetText(char* text,int selection) {
        if (selection > statesCount) return;
        char buffer[10];
        snprintf(buffer, 10, "%2d. ",selection);
        strncpy(text, buffer, 10);
        for (int i = 0;i < jointCount;i++) {
            snprintf(buffer, 10, "%7.3f, ", c[(selection - 1) * jointCount + i]);
            strncat(text, buffer, 10);
        }
        text[strnlen(text, 128) - 2] = '\0';
    }
    const int delay = 60;
    int frames;
    int currentState = 0;
    int statesCount = 0;
    int jointCount;
    std::vector<float> c;
};

class GUI {
public:
    bool teachMode = false; 
    bool workMode = false;

    bool JointPositionBoxEditMode = false;
    float JointPositionBoxValue;
    
    Rectangle SavedStatesPanelView = { 0, 0, 0, 0 };
    Vector2 SavedStatesPanelOffset = { 0, 0 };

    void Draw(JointType jt, SavedStates s) {
    // okno zawierające informacje o położeniu (rozstawie) złącza
        const char* text[] = { "Kąt obrotu [°]:","Przesunięcie:","Rozstaw:" };
        Rectangle JointPositionBoxBounds = { GetScreenWidth() / 2.f, 10, 120, 24 };
        GuiFloatBox(JointPositionBoxBounds, text[jt], &JointPositionBoxValue, -170, 170, JointPositionBoxEditMode);
        // gui w trybie nauki/pracy
        if (teachMode || workMode) {
            Rectangle SavedStatesPanelBounds = { 24,GetScreenHeight() / 2.f - 300, 400, 600 };
            Rectangle SavedStatesPanelContent = SavedStatesPanelBounds;
            SavedStatesPanelContent.width -= 16;
            SavedStatesPanelContent.height = 24 * s.statesCount;
            GuiScrollPanel(SavedStatesPanelBounds, NULL, SavedStatesPanelContent, &SavedStatesPanelOffset, &SavedStatesPanelView);
            for (int i = 1;i < s.statesCount + 1;i++) {
                Color clr = (s.currentState == i) ? YELLOW : BLACK;
                char buffer[128];
                s.GetText(buffer, i);
                Rectangle textBounds = { SavedStatesPanelContent.x, SavedStatesPanelOffset.y + SavedStatesPanelContent.y + 24 * (i - 1), SavedStatesPanelContent.width, 24 };
                if (textBounds.y >= SavedStatesPanelBounds.y && textBounds.y < SavedStatesPanelBounds.y + SavedStatesPanelBounds.height - 13) {
                    GuiDrawText(buffer, textBounds, TEXT_ALIGN_LEFT, clr);
                }
            }
        }
    }
    void Update(SavedStates& s) {
        //sterowanie GUI
        if (IsKeyPressed(KEY_ENTER) && !workMode) JointPositionBoxEditMode = !JointPositionBoxEditMode;
        if (IsKeyPressed(KEY_U)) {
            teachMode = !teachMode;
            if (!teachMode) {
                workMode = false;
                s.c.clear();
                s.statesCount = 0;
                s.frames = 0;
            }
        }
        if (teachMode && IsKeyPressed(KEY_P)) {
            workMode = !workMode;
            if (!workMode) s.currentState = 0;
        }
    }
    GUI() {
        //wczytywanie czcionki
        const int codepointCount = 0x0FFF;
        int codepoints[codepointCount];
        for (int i = 0;i < codepointCount;i++) {
            codepoints[i] = i;
        }
        font = LoadFontEx("Roboto_Condensed-Bold.ttf", 24, codepoints, codepointCount);
        GuiSetFont(font);
        GuiSetStyle(DEFAULT, TEXT_SIZE, 24);
    }
    ~GUI() {
        UnloadFont(font);
    }
    Font font;
};

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 800, "robot"); //inicjalizacja okna
    SetTargetFPS(60);
    MaximizeWindow();

    GUI gui;

    Shader shader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
    clCamera CamInstance({4.0f, 2.0f, 4.0f});
    RobotArm robot("models/robots/robot.glb", shader); //wczytywanie modelu robota z plików glb
    robot.LoadDevice("models/devices/manipulator.glb");

    SavedStates savedStates;
    savedStates.jointCount = robot.model.boneCount - 1;

    int selection = 1;
    const int maxSelection = robot.model.boneCount - 1;

    bool cameraMovementEnabled = true;
    DisableCursor();

    while (!WindowShouldClose()) {
        if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
            cameraMovementEnabled = !cameraMovementEnabled; // przełączanie trybu sterowania kamerą
            (cameraMovementEnabled) ? DisableCursor() : EnableCursor();
        }
        if (cameraMovementEnabled) {
            CamInstance.Update();
        }
        if (IsKeyPressed(KEY_PAGE_UP)) {
            // zmiana wyboru i podświetlenia złącza
            selection = (selection == maxSelection) ? 1 : selection + 1;
            gui.JointPositionBoxEditMode = false;
        }
        if (IsKeyPressed(KEY_PAGE_DOWN)) {
            selection = (selection == 1) ? maxSelection : selection - 1;
            gui.JointPositionBoxEditMode = false;
        }
        if (!gui.JointPositionBoxEditMode && !gui.workMode) {
            if (IsKeyPressed(KEY_EQUAL)) {
                // ruch złączem
                robot.MoveJointDiscrete(selection, 1);
            }
            if (IsKeyPressed(KEY_MINUS)) {
                robot.MoveJointDiscrete(selection, -1);
            }
        }
        if (gui.teachMode && !gui.workMode) {
            // tryb nauki
            if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
                savedStates.Save(robot);
            }
            if (IsKeyPressed(KEY_DELETE)) {
                savedStates.Delete();
            }
        }
        gui.Update(savedStates);
        gui.JointPositionBoxValue = robot.targetPositions[selection];
        robot.UpdateJointsSmooth(0.15f);

        //tryb pracy
        if (gui.workMode) {
            savedStates.frames += 1;
            savedStates.frames %= savedStates.delay;
            if (savedStates.frames == 0) {
                savedStates.currentState = (savedStates.currentState == savedStates.statesCount) ? 1 : savedStates.currentState + 1;
                for (int i = 0;i < savedStates.jointCount;i++) {
                    robot.UpdateTargetPosition(i + 1, savedStates.c[i + savedStates.jointCount * (savedStates.currentState - 1)]);
                }
            }
        }

        BeginDrawing();
            ClearBackground(BLACK);
        
            BeginMode3D(CamInstance.parameters);
                DrawGrid(100, 1.0f); // siatka i układ współrzędnych dla lepszej widoczności
                DrawLine3D({0, 0, 0}, {100, 0, 0}, RED);    // X
                DrawLine3D({0, 0, 0}, {0, 100, 0}, GREEN);  // Y
                DrawLine3D({0, 0, 0}, {0, 0, 100}, BLUE);   // Z
                robot.Draw(selection, CamInstance.parameters, shader);
            EndMode3D();

            gui.Draw(robot.jointTypes[selection], savedStates);
        EndDrawing();
        
        robot.targetPositions[selection] = gui.JointPositionBoxValue;
    }

    UnloadShader(shader);
    CloseWindow();
    return 0;
}