/*

TODO:
-dodać sterowanie
    -do wpisanych koordynatów
-dodać animacje przemieszczania do nowej pozycji
-dodać tryby uczenia i pracy
-dodać kolizje
-dodać interakcje z elementem otoczenia
-dodać oświetlenie 
-GUI
Opcjonalnie:
-dodać możliwość wczytywania osi obrotu/przesuwu(?) i ograniczeń ruchu z pliku
-więcej modeli robotów(cylindryczny, polarny, itp.)
-model rzeczywistego robota

Sterowanie:
= zwiększ nastawę złącza
- zmniejsz nastawę złącza
. wybierz kolejne złącze
, wybierz poprzednie złącze
E ruch kamery do góry
Q ruch kamery w dół

*/

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <memory>
#include <vector>


Matrix MatrixTranslate(Vector3 translation) {
    return MatrixTranslate(translation.x, translation.y, translation.z);
}

Matrix DHtoMatrix(Vector4 DH) {
    Matrix result;
    Matrix RT = MatrixMultiply(MatrixRotateX(DH.x), MatrixTranslate(DH.y, 0, 0));
    Matrix TR = MatrixMultiply(MatrixTranslate(0, DH.z, 0), MatrixRotateY(DH.w));
    result = MatrixMultiply(RT, TR);
    return result;
}

class clCamera {
public:
    clCamera(Vector3 pos) {
        parameters.position = pos;
        parameters.target = Vector3Add(pos, {0, 0, -1});
        parameters.up = {0, 1, 0};
        parameters.fovy = 45.0f;
        parameters.projection = CAMERA_PERSPECTIVE;

        SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
        DisableCursor();
    }

    void Update() {
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

class RobotArm {
public:
    RobotArm(const char* fileName, Shader& shaderRef) : shader(shaderRef) {
        model = LoadModel(fileName);
        absoluteTransforms.resize(model.meshCount);
        DHparameters.resize(model.meshCount);
        absoluteTransforms[0] = MatrixTranslate(model.bindPose[0].translation);
        DHparameters[0] = {0, 0, 0, 0};

        for (int i = 1; i < model.meshCount; i++) {
            absoluteTransforms[i] = MatrixTranslate(model.bindPose[i].translation);
            DHparameters[i] = {0, 0, model.bindPose[i].translation.y - model.bindPose[i - 1].translation.y, 0};
        }

        for (int i = 0; i < model.materialCount; i++) {
            model.materials[i].shader = shader;
        }
    }

    ~RobotArm() {
        UnloadModel(model);
    }

    void Draw(int selection, const Camera3D& cam, Shader& shader) {
        Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
        Matrix projection = MatrixPerspective(cam.fovy * DEG2RAD, (float)GetScreenWidth() / GetScreenHeight(), 0.01f, 1000.0f);

        for (int i = 0; i < model.meshCount; i++) {
            Matrix mModel = absoluteTransforms[i];
            Matrix mvp = MatrixMultiply(MatrixMultiply(mModel, view), projection);

            SetShaderValue(shader, GetShaderLocation(shader, "mvp"), &mvp, 4);
            SetShaderValue(shader, GetShaderLocation(shader, "matModel"), &mModel, 4);

            Vector3 lightDir = Vector3Normalize({-0.5f, -1.0f, -0.3f});
            SetShaderValue(shader, GetShaderLocation(shader, "lightDir"), &lightDir, SHADER_UNIFORM_VEC3);

            Color clr = (i == selection) ? YELLOW : WHITE;
            Vector4 baseColor = { clr.r / 255.0f, clr.g / 255.0f, clr.b / 255.0f, clr.a / 255.0f };
            SetShaderValue(shader, GetShaderLocation(shader, "baseColor"), &baseColor, SHADER_UNIFORM_VEC4);

            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[0]], mModel);

            rlEnableWireMode();
            model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[0]], mModel);
            rlDisableWireMode();
        }
    }

    void MoveJoint(int selection, int direction) {
        DHparameters[selection].x += direction * DEG2RAD * 5;
        absoluteTransforms[0] = DHtoMatrix(DHparameters[0]);

        for (int i = 1; i < model.meshCount; i++) {
            absoluteTransforms[i] = MatrixMultiply(DHtoMatrix(DHparameters[i]), absoluteTransforms[i - 1]);
        }
    }

    Model model;
    std::vector<Matrix> absoluteTransforms;
    std::vector<Vector4> DHparameters;
    Shader& shader;
};

int main() {
    InitWindow(800, 800, "robot");
    SetTargetFPS(60);

    Shader shader = LoadShaderFromMemory(vertexShaderCode, fragmentShaderCode);
    clCamera CamInstance({4.0f, 2.0f, 4.0f});
    RobotArm robot("models/robot.glb", shader);

    int selection = 1;
    const int maxSelection = robot.model.meshCount - 1;

    while (!WindowShouldClose()) {
        CamInstance.Update();

        if (IsKeyPressed(KEY_PERIOD)) {
            selection = (selection == maxSelection) ? 1 : selection + 1;
        }
        if (IsKeyPressed(KEY_COMMA)) {
            selection = (selection == 1) ? maxSelection : selection - 1;
        }
        if (IsKeyPressed(KEY_EQUAL)) {
            robot.MoveJoint(selection, 1);
        }
        if (IsKeyPressed(KEY_MINUS)) {
            robot.MoveJoint(selection, -1);
        }

        BeginDrawing();
            ClearBackground(BLACK);
            BeginMode3D(CamInstance.parameters);
                DrawGrid(100, 1.0f);
                DrawLine3D({0, 0, 0}, {100, 0, 0}, RED);    // X
                DrawLine3D({0, 0, 0}, {0, 100, 0}, GREEN);  // Y
                DrawLine3D({0, 0, 0}, {0, 0, 100}, BLUE);   // Z
                robot.Draw(selection, CamInstance.parameters, shader);
            EndMode3D();
        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
    return 0;
}
