

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <conio.h>
  #define CLEAR "cls"
  static int getch_wrap(void) { return _getch(); }
#else
  #include <termios.h>
  #include <unistd.h>
  #define CLEAR "clear"
  static int getch_wrap(void) {
      struct termios oldt, newt;
      int ch;
      tcgetattr(STDIN_FILENO, &oldt);
      newt = oldt;
      newt.c_lflag &= ~(ICANON | ECHO);
      tcsetattr(STDIN_FILENO, TCSANOW, &newt);
      ch = getchar();
      tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
      return ch;
  }
#endif

/* ?? Constantes ?? */
#define MAX_W    25
#define MAX_H    25
#define MAX_MON  20

#define WEAPON_SWORD  0
#define WEAPON_BOW    1
#define WEAPON_STAFF  2

#define PHASE_MENU     0
#define PHASE_VILLAGE  1
#define PHASE_FLOOR1   2
#define PHASE_FLOOR2   3
#define PHASE_FLOOR3   4
#define PHASE_WIN      5
#define PHASE_GAMEOVER 6
#define PHASE_TUTORIAL 7
#define PHASE_CREDITS  8

/* ?? Estruturas ?? */
typedef struct {
    int  x, y;
    int  alive;
    char type;   /* 'X','Y','Z','N' */
    int  hp;
    int  bdir;   /* boss patrol dir */
    int  btimer;
} Entity;

typedef struct {
    int  x, y;
    char dir;
    int  lives;
    int  weapon;
    int  keys;
} Player;

typedef struct {
    int    w, h;
    char   cells[MAX_H][MAX_W];
    char   orig[MAX_H][MAX_W];
    Entity monsters[MAX_MON];
    int    nmon;
    int    btn_pressed;
} Map;

/* ?? Globais ?? */
static int    phase = PHASE_MENU;
static Player player;
static Map    cur_map;
static char   msg[128];
static int    turns = 0;

/* ?? Forward declarations ?? */
static void build_village(void);
static void build_floor1(void);
static void build_floor2(void);
static void build_floor3(void);
static void draw(void);
static void handle_input(void);
static void move_monsters(void);
static void check_monster_contact(void);
static void reset_phase(void);
static void go_phase(int p);
static int  cell_blocking(char c);
static void do_attack(void);
static void do_interact(void);

/* ?? Utilitarios ?? */
static void cls(void) { system(CLEAR); }

static int cell_blocking(char c) {
    return (c=='*' || c=='k' || c=='D');
}

static void dir_delta(char dir, int *dr, int *dc) {
    *dr = 0; *dc = 0;
    if      (dir=='^') *dr = -1;
    else if (dir=='v') *dr =  1;
    else if (dir=='<') *dc = -1;
    else if (dir=='>') *dc =  1;
}

/* ?? Construcao dos mapas ?? */

/* Copia string de largura fixa para linha do mapa */
static void set_row(Map *m, int r, const char *s) {
    for (int c = 0; c < m->w && s[c]; c++)
        m->cells[r][c] = s[c];
}

static void border(Map *m) {
    for (int c = 0; c < m->w; c++) {
        m->cells[0][c]      = '*';
        m->cells[m->h-1][c] = '*';
    }
    for (int r = 0; r < m->h; r++) {
        m->cells[r][0]      = '*';
        m->cells[r][m->w-1] = '*';
    }
}

/* ?? Vila ?? */
static void build_village(void) {
    Map *m = &cur_map;
    m->w = 10; m->h = 10;
    m->nmon = 0; m->btn_pressed = 0;

    /* Fill with spaces first */
    for (int r = 0; r < 10; r++)
        for (int c = 0; c < 10; c++)
            m->cells[r][c] = ' ';

    border(m);

    /* Entrada da masmorra (escada no lado direito, linha 5) */
    m->cells[5][9] = 'L';

    /* NPC como entidade — celula fica espaco */
    m->monsters[0] = (Entity){3, 3, 1, 'N', 1, 0, 0};
    m->nmon = 1;

    player.x = 2; player.y = 7; player.dir = '>';
    memcpy(m->orig, m->cells, sizeof(m->cells));
}

/* ?? Andar 1 ?? */
static void build_floor1(void) {
    Map *m = &cur_map;
    m->w = 10; m->h = 10;
    m->nmon = 0; m->btn_pressed = 0;

    const char *rows[10] = {
        "**********",
        "*        *",
        "*  kkk   *",
        "*        *",
        "* @      *",
        "* ****   *",
        "* *D     *",
        "* *      *",
        "* *    L *",
        "**********"
    };
    for (int r = 0; r < 10; r++) set_row(m, r, rows[r]);

    player.x = 1; player.y = 1; player.dir = '>';
    player.keys = 0;
    memcpy(m->orig, m->cells, sizeof(m->cells));
}

/* ?? Andar 2 ?? */
static void build_floor2(void) {
    Map *m = &cur_map;
    m->w = 15; m->h = 15;
    m->nmon = 0; m->btn_pressed = 0;

    const char *rows[15] = {
        "***************",
        "*             *",
        "*  ###        *",
        "*             *",
        "* @    k      *",
        "* ******      *",
        "* *    *      *",
        "* * X  *   @  *",
        "* * D         *",
        "* * ####      *",
        "* * O         *",
        "*             *",
        "*          L  *",
        "*             *",
        "***************"
    };
    for (int r = 0; r < 15; r++) set_row(m, r, rows[r]);

    /* Monster Tipo 1 at the X position in the map */
    m->monsters[0] = (Entity){4, 7, 1, 'X', 2, 0, 0};
    m->nmon = 1;
    /* Remove the 'X' from cell so entity handles rendering */
    m->cells[7][4] = ' ';

    player.x = 1; player.y = 1; player.dir = '>';
    player.keys = 0;
    memcpy(m->orig, m->cells, sizeof(m->cells));
}

/* ?? Andar 3 (Boss) ?? */
static void build_floor3(void) {
    Map *m = &cur_map;
    m->w = 25; m->h = 25;
    m->nmon = 0; m->btn_pressed = 0;

    for (int r = 0; r < 25; r++)
        for (int c = 0; c < 25; c++)
            m->cells[r][c] = ' ';
    border(m);

    /* Camara 1 - top-left (chave 1) */
    for (int c = 4; c <= 10; c++) { m->cells[3][c]='*'; m->cells[9][c]='*'; }
    for (int r = 3; r <= 9;  r++) { m->cells[r][4]='*'; m->cells[r][10]='*'; }
    m->cells[9][7]  = 'D'; /* porta saida camara 1 */
    m->cells[6][10] = ' '; /* passagem lateral */
    m->cells[6][7]  = '@'; /* chave 1 */

    /* Camara 2 - top-right (chave 2) */
    for (int c = 14; c <= 21; c++) { m->cells[2][c]='*'; m->cells[8][c]='*'; }
    for (int r = 2;  r <= 8;  r++) { m->cells[r][14]='*'; m->cells[r][21]='*'; }
    m->cells[8][17] = 'D'; /* porta saida camara 2 */
    m->cells[5][17] = '@'; /* chave 2 */

    /* Chave 3 - area central */
    m->cells[12][12] = '@';

    /* Porta boss */
    m->cells[15][12] = 'D';

    /* Arena do boss */
    for (int c = 7; c <= 18; c++) { m->cells[16][c]='*'; m->cells[23][c]='*'; }
    for (int r = 16; r <= 23; r++) { m->cells[r][7]='*'; m->cells[r][18]='*'; }
    m->cells[16][12] = ' '; /* entrada arena */

    /* Escada dentro da arena (so acessivel apos boss morrer) */
    m->cells[22][12] = 'L';

    /* Espinhos no corredor */
    m->cells[11][3]='#'; m->cells[11][4]='#';
    m->cells[12][3]='#';
    m->cells[13][3]='#'; m->cells[13][4]='#';

    /* Caixas */
    m->cells[2][2]='k'; m->cells[3][2]='k';
    m->cells[10][11]='k';

    /* Botao (invoca monstro extra) */
    m->cells[11][10]='O';

    /* Monstros */
    m->monsters[0] = (Entity){2,  12, 1, 'X', 2, 0, 0}; /* Tipo 1 */
    m->monsters[1] = (Entity){12, 19, 1, 'Y', 3, 0, 0}; /* Tipo 2 */
    m->monsters[2] = (Entity){10, 19, 1, 'Y', 3, 0, 0}; /* Tipo 2 */
    m->monsters[3] = (Entity){12, 20, 1, 'Z', 6, 1, 0}; /* Boss */
    m->nmon = 4;

    player.x = 2; player.y = 2; player.dir = '>';
    player.keys = 0;
    memcpy(m->orig, m->cells, sizeof(m->cells));
}

/* ?? Reset de fase (perde vida) ?? */
static void reset_phase(void) {
    int lives  = player.lives - 1;
    int weapon = player.weapon;
    if (lives <= 0) { go_phase(PHASE_GAMEOVER); return; }
    player.lives  = lives;
    player.weapon = weapon;
    switch (phase) {
        case PHASE_VILLAGE: build_village(); break;
        case PHASE_FLOOR1:  build_floor1();  break;
        case PHASE_FLOOR2:  build_floor2();  break;
        case PHASE_FLOOR3:  build_floor3();  break;
    }
    snprintf(msg, sizeof(msg), "Perdeu uma vida! Vidas restantes: %d", lives);
}

static void go_phase(int p) {
    phase    = p;
    msg[0]   = '\0';
    turns    = 0;
    switch (p) {
        case PHASE_VILLAGE: build_village(); break;
        case PHASE_FLOOR1:  build_floor1();  break;
        case PHASE_FLOOR2:  build_floor2();  break;
        case PHASE_FLOOR3:  build_floor3();  break;
        default: break;
    }
}

/* ?? Desenho ?? */
static void draw(void) {
    cls();
    Map *m = &cur_map;

    /* Cabecalho */
    const char *fname[] = {"Vila","Andar 1","Andar 2","Andar 3 - Boss"};
    int fi = phase - PHASE_VILLAGE;
    if (fi < 0 || fi > 3) fi = 0;
    printf("=== DUNGEON CRAWLER | %s ===\n", fname[fi]);

    /* Copiar mapa para buffer de exibicao */
    char disp[MAX_H][MAX_W];
    memcpy(disp, m->cells, sizeof(m->cells));

    /* Colocar monstros no buffer */
    for (int i = 0; i < m->nmon; i++) {
        Entity *e = &m->monsters[i];
        if (!e->alive) continue;
        if (e->x >= 0 && e->x < m->w && e->y >= 0 && e->y < m->h)
            disp[e->y][e->x] = e->type;
    }

    /* Colocar jogador no buffer (por cima de tudo) */
    disp[player.y][player.x] = player.dir;

    /* Imprimir */
    for (int r = 0; r < m->h; r++) {
        printf(" ");
        for (int c = 0; c < m->w; c++)
            putchar(disp[r][c]);
        putchar('\n');
    }

    /* HUD - apenas ASCII */
    const char *wname[] = {"Espada", "Arco e Flecha", "Cajado"};
    printf("\nVidas: ");
    for (int i = 0; i < player.lives; i++) printf("[v]");
    printf("  | Arma: %s | Chaves: %d\n", wname[player.weapon], player.keys);
    if (msg[0]) printf(">> %s\n", msg);
    printf("[wasd] mover  [i] interagir  [o] atacar  [q] menu\n> ");
    fflush(stdout);
}

/* ?? Area de ataque ?? */
static int in_attack_area(int pr, int pc, char dir, int weapon, int tr, int tc) {
    int dr, dc;
    dir_delta(dir, &dr, &dc);

    if (weapon == WEAPON_SWORD) {
        /* 3x2: 2 de profundidade, 3 de largura */
        int pr2, pc2;
        if (dr != 0) { pr2 = 0; pc2 = 1; }
        else         { pr2 = 1; pc2 = 0; }
        for (int depth = 1; depth <= 2; depth++)
            for (int side = -1; side <= 1; side++) {
                if (pr + dr*depth + pr2*side == tr &&
                    pc + dc*depth + pc2*side == tc) return 1;
            }
    } else if (weapon == WEAPON_BOW) {
        /* linha reta, 4 celulas */
        for (int d = 1; d <= 4; d++)
            if (pr + dr*d == tr && pc + dc*d == tc) return 1;
    } else {
        /* STAFF: 8 celulas adjacentes */
        for (int rr = -1; rr <= 1; rr++)
            for (int cc = -1; cc <= 1; cc++)
                if ((rr || cc) && pr+rr == tr && pc+cc == tc) return 1;
    }
    return 0;
}

static void do_attack(void) {
    Map *m = &cur_map;
    int pr = player.y, pc = player.x;
    int hit = 0;

    /* Destruir caixas */
    for (int r = 0; r < m->h; r++)
        for (int c = 0; c < m->w; c++)
            if (m->cells[r][c]=='k' &&
                in_attack_area(pr,pc,player.dir,player.weapon,r,c)) {
                m->cells[r][c] = ' ';
                hit = 1;
            }

    /* Acertar monstros */
    for (int i = 0; i < m->nmon; i++) {
        Entity *e = &m->monsters[i];
        if (!e->alive || e->type=='N') continue;
        if (in_attack_area(pr,pc,player.dir,player.weapon,e->y,e->x)) {
            e->hp--;
            hit = 1;
            if (e->hp <= 0) {
                e->alive = 0;
                if (e->type == 'Z') { go_phase(PHASE_WIN); return; }
                snprintf(msg, sizeof(msg), "Monstro eliminado!");
            } else {
                snprintf(msg, sizeof(msg), "Acertou! HP inimigo: %d", e->hp);
            }
        }
    }
    if (!hit) snprintf(msg, sizeof(msg), "Ataque no vazio.");
}

/* ?? Interacao ?? */
static void do_interact(void) {
    Map *m = &cur_map;
    int dr, dc;
    dir_delta(player.dir, &dr, &dc);
    int tr = player.y + dr, tc = player.x + dc;
    if (tr < 0 || tr >= m->h || tc < 0 || tc >= m->w) return;

    /* Verificar entidade NPC primeiro */
    for (int i = 0; i < m->nmon; i++) {
        Entity *e = &m->monsters[i];
        if (e->alive && e->type=='N' && e->y==tr && e->x==tc) {
            cls();
            printf("\n=== NPC da Vila - Escolha sua arma ===\n\n");
            printf("  1) Espada       - area 3x2 a frente\n");
            printf("  2) Arco e Flecha - linha reta, 4 celulas\n");
            printf("  3) Cajado       - 8 celulas ao redor\n\n");
            printf("> ");
            fflush(stdout);
            int ch = getch_wrap();
            if      (ch=='1') { player.weapon=WEAPON_SWORD; snprintf(msg,sizeof(msg),"Espada equipada!"); }
            else if (ch=='2') { player.weapon=WEAPON_BOW;   snprintf(msg,sizeof(msg),"Arco equipado!"); }
            else if (ch=='3') { player.weapon=WEAPON_STAFF; snprintf(msg,sizeof(msg),"Cajado equipado!"); }
            else               snprintf(msg,sizeof(msg),"Arma mantida.");
            return;
        }
    }

    char cell = m->cells[tr][tc];

    if (cell == '@') {
        player.keys++;
        m->cells[tr][tc] = ' ';
        snprintf(msg, sizeof(msg), "Chave coletada! Total: %d", player.keys);
    } else if (cell == 'D') {
        if (player.keys > 0) {
            player.keys--;
            m->cells[tr][tc] = '=';
            snprintf(msg, sizeof(msg), "Porta aberta!");
        } else {
            snprintf(msg, sizeof(msg), "Precisa de uma chave!");
        }
    } else if (cell == 'O') {
        m->btn_pressed = 1;
        m->cells[tr][tc] = '.';
        if (phase == PHASE_FLOOR2) {
            /* Abre passagem bloqueada */
            m->cells[8][2] = ' ';
            snprintf(msg, sizeof(msg), "Botao! Passagem aberta.");
        } else if (phase == PHASE_FLOOR3) {
            /* Invoca monstro extra */
            for (int i = 0; i < MAX_MON; i++) {
                if (!m->monsters[i].alive) {
                    m->monsters[i] = (Entity){5, 11, 1, 'X', 2, 0, 0};
                    if (i >= m->nmon) m->nmon = i + 1;
                    break;
                }
            }
            snprintf(msg, sizeof(msg), "Botao! Um monstro apareceu!");
        }
    } else if (cell == 'L') {
        if      (phase == PHASE_VILLAGE) go_phase(PHASE_FLOOR1);
        else if (phase == PHASE_FLOOR1)  go_phase(PHASE_FLOOR2);
        else if (phase == PHASE_FLOOR2)  go_phase(PHASE_FLOOR3);
    } else {
        snprintf(msg, sizeof(msg), "Nada aqui.");
    }
}

/* ?? IA dos monstros ?? */
static void move_monsters(void) {
    Map *m = &cur_map;
    static const int dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};

    for (int i = 0; i < m->nmon; i++) {
        Entity *e = &m->monsters[i];
        if (!e->alive || e->type=='N') continue;

        int nr = e->y, nc = e->x;

        if (e->type == 'X') {
            /* Aleatorio */
            int d = rand() % 4;
            nr = e->y + dirs[d][0];
            nc = e->x + dirs[d][1];
        } else if (e->type == 'Y') {
            /* Perseguicao simples */
            int dy = player.y - e->y;
            int dx = player.x - e->x;
            if (abs(dy) >= abs(dx)) nr = e->y + (dy > 0 ? 1 : -1);
            else                    nc = e->x + (dx > 0 ? 1 : -1);
        } else if (e->type == 'Z') {
            /* Boss: patrulha + teleporte a cada 5 turnos */
            e->btimer++;
            if (e->btimer % 5 == 0) {
                int br = player.y + (rand()%3) - 1;
                int bc = player.x + (rand()%3) - 1;
                if (br < 17) br = 17;
                if (br > 22) br = 22;
                if (bc < 8)  bc = 8;
                if (bc > 17) bc = 17;
                if (m->cells[br][bc]==' ' || m->cells[br][bc]=='.') {
                    nr = br; nc = bc;
                }
            } else {
                nc = e->x + e->bdir;
                nr = e->y;
                if (nc < 8 || nc > 17 || cell_blocking(m->cells[nr][nc])) {
                    e->bdir = -e->bdir;
                    nc = e->x + e->bdir;
                }
                if (e->bdir == 0) e->bdir = 1;
            }
        }

        if (nr < 0 || nr >= m->h || nc < 0 || nc >= m->w) continue;
        char dest = m->cells[nr][nc];
        if (dest=='*' || dest=='k' || dest=='D' || dest=='#') continue;

        /* Nao sobrepor outro monstro */
        int occ = 0;
        for (int j = 0; j < m->nmon; j++) {
            if (j==i) continue;
            if (m->monsters[j].alive && m->monsters[j].y==nr && m->monsters[j].x==nc) {
                occ = 1; break;
            }
        }
        if (!occ) { e->y = nr; e->x = nc; }
    }
}

/* ?? Verificar dano ao jogador ?? */
static void check_monster_contact(void) {
    Map *m = &cur_map;

    if (m->cells[player.y][player.x] == '#') {
        snprintf(msg, sizeof(msg), "Voce pisou num espinho!");
        reset_phase(); return;
    }
    for (int i = 0; i < m->nmon; i++) {
        Entity *e = &m->monsters[i];
        if (!e->alive || e->type=='N') continue;
        if (e->y==player.y && e->x==player.x) {
            snprintf(msg, sizeof(msg), "Um monstro te tocou!");
            reset_phase(); return;
        }
    }
}

/* ?? Input ?? */
static void handle_input(void) {
    Map *m = &cur_map;
    msg[0] = '\0';

    int ch = getch_wrap();
    if (ch == 'q') { go_phase(PHASE_MENU); return; }

    int nr = player.y, nc = player.x;
    char new_dir = player.dir;

    switch (ch) {
        case 'w': nr--; new_dir='^'; break;
        case 's': nr++; new_dir='v'; break;
        case 'a': nc--; new_dir='<'; break;
        case 'd': nc++; new_dir='>'; break;
        case 'o':
            player.dir = new_dir;
            do_attack();
            if (phase >= PHASE_VILLAGE && phase <= PHASE_FLOOR3) {
                turns++; move_monsters(); check_monster_contact();
            }
            return;
        case 'i':
            player.dir = new_dir;
            do_interact();
            return;
        default:
            return;
    }

    player.dir = new_dir;

    if (nr < 0 || nr >= m->h || nc < 0 || nc >= m->w) return;
    char dest = m->cells[nr][nc];

    /* Verificar entidade bloqueando */
    int occ = 0;
    for (int i = 0; i < m->nmon; i++) {
        Entity *e = &m->monsters[i];
        if (e->alive && e->y==nr && e->x==nc) { occ = 1; break; }
    }

    if (!cell_blocking(dest) && !occ) {
        player.y = nr; player.x = nc;
    }

    turns++;
    move_monsters();
    check_monster_contact();
}

/* ?? Telas ?? */
static void show_menu(void) {
    cls();
    printf("\n");
    printf("  +------------------------------+\n");
    printf("  |   CRONICAS DA MASMORRA       |\n");
    printf("  +------------------------------+\n\n");
    printf("  1) Jogar\n");
    printf("  2) Tutorial\n");
    printf("  3) Sair\n\n");
    printf("  > ");
    fflush(stdout);
    int ch = getch_wrap();
    if (ch == '1') {
        player.lives  = 3;
        player.weapon = WEAPON_SWORD;
        player.keys   = 0;
        go_phase(PHASE_VILLAGE);
    } else if (ch == '2') {
        go_phase(PHASE_TUTORIAL);
    } else if (ch == '3') {
        go_phase(PHASE_CREDITS);
    }
}

static void show_tutorial(void) {
    cls();
    printf("\n=== HISTORIA ===\n");
    printf("Um mal antigo desperta nas profundezas da masmorra.\n");
    printf("Voce, o ultimo heroi, deve descer os tres andares\n");
    printf("e derrotar o Boss das Trevas.\n\n");
    printf("=== SIMBOLOS ===\n");
    printf("  ^ v < >   Jogador (direcao)\n");
    printf("  *         Parede\n");
    printf("  #         Espinho (mata ao pisar)\n");
    printf("  k         Caixa (destruivel com ataque)\n");
    printf("  O         Botao\n");
    printf("  D         Porta fechada\n");
    printf("  =         Porta aberta\n");
    printf("  @         Chave\n");
    printf("  L         Escada (proximo andar)\n");
    printf("  X         Monstro Tipo 1 (aleatorio)\n");
    printf("  Y         Monstro Tipo 2 (perseguicao)\n");
    printf("  Z         Boss Final\n");
    printf("  N         NPC\n\n");
    printf("=== CONTROLES ===\n");
    printf("  w/a/s/d   Mover\n");
    printf("  i         Interagir com objeto a frente\n");
    printf("  o         Atacar\n");
    printf("  q         Voltar ao menu\n\n");
    printf("Pressione qualquer tecla...\n");
    fflush(stdout);
    getch_wrap();
    go_phase(PHASE_MENU);
}

static void show_credits(void) {
    cls();
    printf("\n");
    printf("  +------------------------------+\n");
    printf("  |          CREDITOS            |\n");
    printf("  |                              |\n");
    printf("  |  Desenvolvido por:           |\n");
    printf("  |  Igor Aquino, Hugo Victor.   |\n");
    printf("  |                              |\n");
    printf("  |  Obrigado por jogar!         |\n");
    printf("  +------------------------------+\n\n");
    printf("Pressione qualquer tecla para sair...\n");
    fflush(stdout);
    getch_wrap();
    exit(0);
}

static void show_gameover(void) {
    cls();
    printf("\n");
    printf("  +------------------------------+\n");
    printf("  |         GAME OVER            |\n");
    printf("  |                              |\n");
    printf("  |  Voce perdeu todas as vidas. |\n");
    printf("  +------------------------------+\n\n");
    printf("Pressione qualquer tecla...\n");
    fflush(stdout);
    getch_wrap();
    go_phase(PHASE_MENU);
}

static void show_win(void) {
    cls();
    printf("\n");
    printf("  +--------------------------------------+\n");
    printf("  |            ** VITORIA! **            |\n");
    printf("  +--------------------------------------+\n");
    printf("  |                                      |\n");
    printf("  |  O Boss das Trevas foi derrotado!    |\n");
    printf("  |                                      |\n");
    printf("  |  A luz voltou ao reino. Os cidadaos  |\n");
    printf("  |  celebram seu nome pelas ruas e      |\n");
    printf("  |  cantos. A masmorra e selada para    |\n");
    printf("  |  sempre. Voce e o heroi da lenda.    |\n");
    printf("  |                                      |\n");
    printf("  |       Obrigado por jogar!            |\n");
    printf("  +--------------------------------------+\n\n");
    printf("Pressione qualquer tecla...\n");
    fflush(stdout);
    getch_wrap();
    go_phase(PHASE_MENU);
}

/* ?? Main ?? */
int main(void) {
    srand((unsigned)time(NULL));
    phase = PHASE_MENU;

    while (1) {
        switch (phase) {
            case PHASE_MENU:     show_menu();     break;
            case PHASE_TUTORIAL: show_tutorial(); break;
            case PHASE_CREDITS:  show_credits();  break;
            case PHASE_GAMEOVER: show_gameover(); break;
            case PHASE_WIN:      show_win();      break;
            default:
                draw();
                handle_input();
                break;
        }
    }
    return 0;
}