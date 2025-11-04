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

// Desenează un "con" cu baza formată din 4 petale Bézier
void drawBezierCone(float L = 3.0f, int samples = 50, float innerR = 0.4f, float outerR = 2.4f, float sweepDeg = 0.0f) {
    std::vector<Point> base;
    base.reserve((size_t)(samples + 1) * 4);

    // O petală cu start/finish la distanța innerR față de axă
    auto petal = generatePetal(L, samples, innerR, outerR, sweepDeg);

    // Rotim petala de 4 ori (0, 90, 180, 270)
    for (int k = 0; k < 4; k++) {
        auto rotated = rotatePetal(petal, k * 90.0f);
        base.insert(base.end(), rotated.begin(), rotated.end());
    }

    // Suprafața laterală: triunghiuri cu vârful în origine (cu normale pentru iluminare)
    glColor3d(0.7, 0.2, 0.8);

    glBegin(GL_TRIANGLES);
    for (size_t i = 0; i < base.size(); i++) {
        size_t j = (i + 1) % base.size();

        // vârfuri (originea și două puncte consecutive din bază)
        Point v0{0.0f, 0.0f, 0.0f};
        const Point& v1 = base[i];
        const Point& v2 = base[j];

        // normale flat: n = normalize( (v2 - v0) x (v1 - v0) ) -> orientare spre exterior
        Point a{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
        Point b{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
        Point n{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 0.0f) { n.x /= len; n.y /= len; n.z /= len; } else { n = {1.0f, 0.0f, 0.0f}; }

        glNormal3f(n.x, n.y, n.z);
        glVertex3f(v0.x, v0.y, v0.z);
        glVertex3f(v1.x, v1.y, v1.z);
        glVertex3f(v2.x, v2.y, v2.z);
    }
    glEnd();

    // Baza (wireframe) fără iluminare pentru claritate
    glDisable(GL_LIGHTING);
    glColor3d(0.2, 0.5, 0.9);
    glBegin(GL_LINE_LOOP);
    for (auto& p : base) {
        glVertex3f(p.x, p.y, p.z);
    }
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

    // Actualizăm poziția sursei de lumină (spațiu lume)
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    GLfloat lightPos[]  = { 5.0f, 8.0f, 7.0f, 1.0f }; // lumină punctuală
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glPopMatrix();

    glPushMatrix();
    glTranslated(0., 0., -6.0);
    glRotated(35., 1., 0., 0.);
    glRotated(-35., 0., 1., 0.);

    // Axe fără iluminare pentru vizibilitate
    glDisable(GL_LIGHTING);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glColor3d(1., 0., 0.); glVertex3d(-5.5, 0., 0.); glVertex3d(5.5, 0., 0.);
    glColor3d(0., 1., 0.); glVertex3d(0., -5.5, 0.); glVertex3d(0., 5.5, 0.);
    glColor3d(0., 0., 1.); glVertex3d(0., 0., -5.5); glVertex3d(0., 0., 5.5);
    glEnd();
    glEnable(GL_LIGHTING);

    // Conuri la capete (au normale interne din GLUT, vor fi iluminate)
    glColor3d(1, 0, 0);
    glPushMatrix(); glTranslated(5.3f, 0.0f, 0.0f); glRotated(90, 0, 1, 0); glutSolidCone(0.1f, 0.2f, 16, 16); glPopMatrix();
    glColor3d(0, 1, 0);
    glPushMatrix(); glTranslated(0.0f, 5.3f, 0.0f); glRotated(-90, 1, 0, 0); glutSolidCone(0.1f, 0.2f, 16, 16); glPopMatrix();
    glColor3d(0, 0, 1);
    glPushMatrix(); glTranslated(0.0f, 0.0f, 5.3f); glutSolidCone(0.1f, 0.2f, 16, 16); glPopMatrix();

    // Conul Bézier cu iluminare (normale setate în drawBezierCone)
    drawBezierCone(3.0f, 60, 0.5f, 2.5f, 0.0f);

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

    // Stare OpenGL
    glEnable(GL_DEPTH_TEST);

    // Iluminare: sursa L0 + material
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);         // normalizare automată (sigură la transformări)
    glShadeModel(GL_SMOOTH);

    // Intensități lumină
    GLfloat L0_amb[]  = { 0.25f, 0.25f, 0.25f, 1.0f };
    GLfloat L0_diff[] = { 0.95f, 0.95f, 0.95f, 1.0f };
    GLfloat L0_spec[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT,  L0_amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  L0_diff);
    glLightfv(GL_LIGHT0, GL_SPECULAR, L0_spec);

    // Material: folosește culoarea curentă pentru ambient+difuz
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    GLfloat matSpec[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);

    glClearColor(1.f, 1.f, 1.f, 1.f);

    glutMainLoop();
    return 0;
}
