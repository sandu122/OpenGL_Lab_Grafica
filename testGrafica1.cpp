#include <windows.h>
#include <GL/freeglut.h>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Point {
    float x, y, z;
};

// --- Interactive rotation state ---
static float g_rotX = 0.0f, g_rotY = 0.0f;
static bool  g_dragging = false;
static int   g_lastX = 0, g_lastY = 0;
static float g_sensitivity = 0.5f;

// Mouse button callback
void OnMouseButton(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) { g_dragging = true; g_lastX = x; g_lastY = y; }
        else                    { g_dragging = false; }
    }
}

// Mouse motion callback (while dragging)
void OnMouseMove(int x, int y) {
    if (!g_dragging) return;
    int dx = x - g_lastX;
    int dy = y - g_lastY;
    g_rotY += dx * g_sensitivity;   // horizontal drag rotates around Y
    g_rotX += dy * g_sensitivity;   // vertical drag rotates around X
    g_lastX = x; g_lastY = y;
    glutPostRedisplay();
}

// Arrow keys for rotation, R to reset
void OnSpecialKey(int key, int, int) {
    const float step = 3.0f;
    switch (key) {
    case GLUT_KEY_LEFT:  g_rotY -= step; break;
    case GLUT_KEY_RIGHT: g_rotY += step; break;
    case GLUT_KEY_UP:    g_rotX -= step; break;
    case GLUT_KEY_DOWN:  g_rotX += step; break;
    }
    glutPostRedisplay();
}
void OnKeyboard(unsigned char key, int, int) {
    if (key == 'r' || key == 'R') { g_rotX = g_rotY = 0.0f; glutPostRedisplay(); }
}

// Funcție pentru evaluarea unei curbe Bézier cubice
Point bezier(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float t) {
    float u = 1 - t;
    float b0 = u * u * u;
    float b1 = 3 * u * u * t;
    float b2 = 3 * u * t * t;
    float b3 = t * t * t;
    return {
        b0 * p0.x + b1 * p1.x + b2 * p2.x + b3 * p3.x,
        b0 * p0.y + b1 * p1.y + b2 * p2.y + b3 * p3.y,
        b0 * p0.z + b1 * p1.z + b2 * p2.z + b3 * p3.z
    };
}

// Generează o petală Bézier în planul YZ (la x = L)
// innerR = distanța minimă față de axa X (offsetul dorit)
// outerR = cât de mult iese petala în exterior (bombare)
// sweepDeg = un mic unghi astfel încât p0 și p3 să fie pe un cerc de rază innerR la unghiuri diferite (opțional)
std::vector<Point> generatePetal(float L, int samples, float innerR, float outerR, float sweepDeg = 0.0f) {
    float sweep = sweepDeg * (float)M_PI / 180.0f;

    // Punctele de start/finish pe cercul de rază innerR în planul YZ
    // p0 la unghi -sweep/2, p3 la +sweep/2 în jurul +Z (y=sin, z=cos)
    Point p0 = { L, innerR * std::sin(-0.5f * sweep), innerR * std::cos(-0.5f * sweep) };
    Point p3 = { L, innerR * std::sin(+0.5f * sweep), innerR * std::cos(+0.5f * sweep) };

    // Puncte de control pentru bombare (simetrice pe Y, împinse pe +Z la outerR)
    Point p1 = { L, +0.6f * outerR, outerR };
    Point p2 = { L, -0.6f * outerR, outerR };

    std::vector<Point> curve;
    curve.reserve(static_cast<size_t>(samples) + 1);
    for (int i = 0; i <= samples; i++) {
        float t = (float)i / samples;
        curve.push_back(bezier(p0, p1, p2, p3, t));
    }
    return curve;
}

// Rotim petala în jurul axei X pentru a obține petale multiple
std::vector<Point> rotatePetal(const std::vector<Point>& petal, float angleDeg) {
    float angle = angleDeg * (float)M_PI / 180.0f;
    float c = std::cos(angle), s = std::sin(angle);
    std::vector<Point> rotated;
    rotated.reserve(petal.size());
    for (auto& p : petal) {
        float y = p.y * c - p.z * s;
        float z = p.y * s + p.z * c;
        rotated.push_back({ p.x, y, z });
    }
    return rotated;
}

// Helper to accumulate and normalize vertex normals
struct AccumNormal { double x=0, y=0, z=0; };
static void addNormal(AccumNormal& acc, const Point& n) { acc.x += n.x; acc.y += n.y; acc.z += n.z; }
static Point normalize(const AccumNormal& acc) {
    double len = std::sqrt(acc.x*acc.x + acc.y*acc.y + acc.z*acc.z);
    if (len <= 1e-9) return {0.f,0.f,1.f};
    return { (float)(acc.x/len), (float)(acc.y/len), (float)(acc.z/len) };
}

static float segLen(const Point& a, const Point& b) {
    float dx=b.x-a.x, dy=b.y-a.y, dz=b.z-a.z;
    return std::sqrt(dx*dx+dy*dy+dz*dz);
}

static std::vector<Point> resampleClosedLoop(const std::vector<Point>& loop, int target) {
    std::vector<Point> out; out.reserve(target);
    const int N = (int)loop.size();
    if (N == 0 || target <= 0) return out;

    // cumulative chord lengths
    std::vector<float> acc(N+1, 0.0f);
    for (int i=0; i<N; ++i) acc[i+1] = acc[i] + segLen(loop[i], loop[(i+1)%N]);
    float total = acc[N];
    if (total <= 0.0f) { // degenerate, just duplicate a point
        out.assign(target, loop[0]); return out;
    }

    // sample uniformly by arc length
    int j = 0;
    for (int k=0; k<target; ++k) {
        float s = (total * k) / target; // target arc-length
        // advance j so that acc[j] <= s < acc[j+1]
        while (j+1 < (int)acc.size() && acc[j+1] < s) ++j;
        float segStart = acc[j];
        float segEnd   = acc[j+1];
        float t = (segEnd > segStart) ? (s - segStart) / (segEnd - segStart) : 0.0f;

        const Point& a = loop[j % N];
        const Point& b = loop[(j+1) % N];
        out.push_back({ a.x + (b.x-a.x)*t, a.y + (b.y-a.y)*t, a.z + (b.z-a.z)*t });
    }
    return out;
}

// Modify the signature to add 'sectors' (last arg). Keep default as -1 to preserve current behavior.
void drawBezierCone(float L = 3.0f, int samples = 50, float innerR = 0.4f, float outerR = 2.4f, float sweepDeg = 0.0f, int layers = -1, int sectors = -1) {
    if (layers < 0) layers = samples;

    // Build the base exactly like before (smooth curve with 'samples')
    std::vector<Point> base;
    base.reserve((size_t)(samples + 1) * 4);
    auto petal = generatePetal(L, samples, innerR, outerR, sweepDeg);
    for (int k = 0; k < 4; k++) {
        auto rotated = rotatePetal(petal, k * 90.0f);
        base.insert(base.end(), rotated.begin(), rotated.end());
    }

    // If a sector count is requested, resample the closed base loop to exactly 'sectors' points
    if (sectors > 0) {
        base = resampleClosedLoop(base, sectors);
    }
    sectors = (int)base.size();

    // Build rings apex->base
    std::vector<std::vector<Point>> rings(layers + 1, std::vector<Point>(sectors));
    for (int r = 0; r <= layers; ++r) {
        float s = (float)r / (float)layers;
        for (int i = 0; i < sectors; ++i) {
            rings[r][i] = { base[i].x * s, base[i].y * s, base[i].z * s };
        }
    }

    // Smooth normals accumulation as in your current file
    std::vector<std::vector<AccumNormal>> vnorm(layers + 1, std::vector<AccumNormal>(sectors));
    auto faceNormal = [](const Point& a, const Point& b, const Point& c) {
        Point u{ b.x - a.x, b.y - a.y, b.z - a.z };
        Point v{ c.x - a.x, c.y - a.y, c.z - a.z };
        Point n{
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        };
        float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
        if (len > 0) { n.x/=len; n.y/=len; n.z/=len; }
        return n;
    };
    for (int r = 0; r < layers; ++r) {
        for (int i = 0; i < sectors; ++i) {
            int inext = (i + 1) % sectors;
            const Point& v00 = rings[r][i];
            const Point& v01 = rings[r][inext];
            const Point& v10 = rings[r+1][i];
            const Point& v11 = rings[r+1][inext];
            Point n1 = faceNormal(v00, v10, v11);
            addNormal(vnorm[r][i], n1);
            addNormal(vnorm[r+1][i], n1);
            addNormal(vnorm[r+1][inext], n1);
            Point n2 = faceNormal(v00, v11, v01);
            addNormal(vnorm[r][i], n2);
            addNormal(vnorm[r+1][inext], n2);
            addNormal(vnorm[r][inext], n2);
        }
    }

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    glColor3d(0.7, 0.2, 0.8);
    glBegin(GL_TRIANGLES);
    for (int r = 0; r < layers; ++r) {
        for (int i = 0; i < sectors; ++i) {
            int inext = (i + 1) % sectors;
            const Point& v00 = rings[r][i];
            const Point& v01 = rings[r][inext];
            const Point& v10 = rings[r+1][i];
            const Point& v11 = rings[r+1][inext];

            Point n00 = normalize(vnorm[r][i]);
            Point n10 = normalize(vnorm[r+1][i]);
            Point n11 = normalize(vnorm[r+1][inext]);
            Point n01 = normalize(vnorm[r][inext]);

            // Tri 1
            glNormal3f(n00.x,n00.y,n00.z); glVertex3f(v00.x,v00.y,v00.z);
            glNormal3f(n10.x,n10.y,n10.z); glVertex3f(v10.x,v10.y,v10.z);
            glNormal3f(n11.x,n11.y,n11.z); glVertex3f(v11.x,v11.y,v11.z);

            // Tri 2
            glNormal3f(n00.x,n00.y,n00.z); glVertex3f(v00.x,v00.y,v00.z);
            glNormal3f(n11.x,n11.y,n11.z); glVertex3f(v11.x,v11.y,v11.z);
            glNormal3f(n01.x,n01.y,n01.z); glVertex3f(v01.x,v01.y,v01.z);
        }
    }
    glEnd();

    glDisable(GL_POLYGON_OFFSET_FILL);

    // Mesh overlay: rings, spokes, diagonals (unchanged logic)
    glDisable(GL_LIGHTING);
    glColor3d(0.1, 0.1, 0.1);
    glLineWidth(1.0f);

    // Inele
    for (int r = 0; r <= layers; ++r) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < sectors; ++i) {
            const Point& v = rings[r][i];
            glVertex3f(v.x, v.y, v.z);
        }
        glEnd();
    }

    // Spițe (apex -> bază pe fiecare sector)
    for (int i = 0; i < sectors; ++i) {
        glBegin(GL_LINE_STRIP);
        for (int r = 0; r <= layers; ++r) {
            const Point& v = rings[r][i];
            glVertex3f(v.x, v.y, v.z);
        }
        glEnd();
    }

    // Diagonale de triangulare pentru fiecare patrulater
    glColor3d(0.2, 0.2, 0.2);
    glBegin(GL_LINES);
    for (int r = 0; r < layers; ++r) {
        for (int i = 0; i < sectors; ++i) {
            int inext = (i + 1) % sectors;
            const Point& v00 = rings[r][i];
            const Point& v11 = rings[r+1][inext];
            glVertex3f(v00.x, v00.y, v00.z);
            glVertex3f(v11.x, v11.y, v11.z);
        }
    }
    glEnd();

    glEnable(GL_LIGHTING);

    // Baza ca wireframe (opțional, neschimbat)
    glDisable(GL_LIGHTING);
    glColor3d(0.2, 0.5, 0.9);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < sectors; ++i) glVertex3f(base[i].x, base[i].y, base[i].z);
    glEnd();
    glEnable(GL_LIGHTING);
}

void resize(int width, int height) {
    GLuint wp = width < height ? width - 20 : height - 20;
    glViewport(10, 10, wp, wp);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(-6.2, 6.2, -6.2, 6.2, 2., 12.);
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Poziția sursei de lumină (fără a schimba parametrii luminii)
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    GLfloat lightPos[]  = { 5.0f, 8.0f, 7.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glPopMatrix();

    glPushMatrix();
    glTranslated(0., 0., -6.0);

    // Orientare inițială
    glRotated(35., 1., 0., 0.);
    glRotated(-35., 0., 1., 0.);

    // Rotiri interactive
    glRotated(g_rotX, 1., 0., 0.);
    glRotated(g_rotY, 0., 1., 0.);

    // Axe fără iluminare pentru vizibilitate
    glDisable(GL_LIGHTING);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glColor3d(1., 0., 0.); glVertex3d(-5.5, 0., 0.); glVertex3d(5.5, 0., 0.);
    glColor3d(0., 1., 0.); glVertex3d(0., -5.5, 0.); glVertex3d(0., 5.5, 0.);
    glColor3d(0., 0., 1.); glVertex3d(0., 0., -5.5); glVertex3d(0., 0., 5.5);
    glEnd();
    glEnable(GL_LIGHTING);

    // Conuri la capete (iluminate)
    glColor3d(1, 0, 0);
    glPushMatrix(); glTranslated(5.3f, 0.0f, 0.0f); glRotated(90, 0, 1, 0); glutSolidCone(0.1f, 0.2f, 16, 16); glPopMatrix();
    glColor3d(0, 1, 0);
    glPushMatrix(); glTranslated(0.0f, 5.3f, 0.0f); glRotated(-90, 1, 0, 0); glutSolidCone(0.1f, 0.2f, 16, 16); glPopMatrix();
    glColor3d(0, 0, 1);
    glPushMatrix(); glTranslated(0.0f, 0.0f, 5.3f); glutSolidCone(0.1f, 0.2f, 16, 16); glPopMatrix();

    // Surface + interior mesh with 7 layers
    drawBezierCone(3.0f, 60 /*samples for curve smoothness*/, 0.5f, 2.5f, 0.0f /*sweep*/, 7 /*layers*/, 96 /*sectors*/);

    glPopMatrix();
    glutSwapBuffers();
}

int main() {
    int argc = 1; char arg0[] = "app"; char* argv[] = { arg0, nullptr };
    glutInit(&argc, argv);
    glutInitWindowPosition(0, 0);
    glutInitWindowSize(600, 600);
    glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
    glutCreateWindow("Con cu baza Bézier");

    glutDisplayFunc(display);
    glutIdleFunc(display);
    glutReshapeFunc(resize);

    // Interacțiune
    glutMouseFunc(OnMouseButton);
    glutMotionFunc(OnMouseMove);
    glutSpecialFunc(OnSpecialKey);
    glutKeyboardFunc(OnKeyboard);

    // Stare OpenGL
    glEnable(GL_DEPTH_TEST);

    // Iluminare existentă (nu modificăm valorile)
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    // Valorile tale curente
    GLfloat L0_amb[]  = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat L0_diff[] = { 0.95f, 0.95f, 0.95f, 1.0f };
    GLfloat L0_spec[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  L0_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  L0_diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, L0_spec);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat matSpec[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

    glClearColor(1.f, 1.f, 1.f, 1.f);

    glutMainLoop();
    return 0;
}
