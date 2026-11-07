/* VELOCITY - arme en main (viewmodel)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void draw_viewmodel(float aspect) {
    glClear(GL_DEPTH_BUFFER_BIT);
    set_frustum(60, aspect, 0.01f, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float hs = hlen(P.vel);
    float bx = (P.grounded && !P.sliding) ? cosf(P.bobT * 0.5f) * 0.012f * fminf(hs / RUN_SPEED, 1) : 0;
    float lower = P.switchT > 0 ? P.switchT / SWITCH_TIME : 0;
    if (P.dead) lower = 1.5f;
    glTranslatef(0.2f + bx + P.swayX, -0.19f + P.bob * 0.3f - lower * 0.25f + P.swayY - P.landDip * 0.2f,
                 -0.42f + P.recoil * 0.06f);
    glScalef(0.5f, 0.5f, 0.5f);
    if (P.switchT > 0 && !P.dead) {                       /* changement d'arme : la nouvelle remonte en pivotant */
        float k = P.switchT / SWITCH_TIME;
        glRotatef(-k * k * 35, 1, 0, 0);
        glRotatef(k * 18, 0, 1, 0);
    }
    glRotatef(P.recoil * 7, 1, 0, 0);
    glRotatef(-P.roll * 0.5f, 0, 0, 1);
    if (P.sliding) glRotatef(-10, 0, 0, 1);

    {                                                     /* le soleil de la scene, ramene dans le repere de la vue */
        V3 L = sun_dir();
        GLfloat lp[4] = { mvMat[0] * L.x + mvMat[4] * L.y + mvMat[8] * L.z, mvMat[1] * L.x + mvMat[5] * L.y + mvMat[9] * L.z,
                          mvMat[2] * L.x + mvMat[6] * L.y + mvMat[10] * L.z, 0 }, lp1[4] = { 0.3f, -0.4f, -0.6f, 0 };
        float k = gxVmSun[0] + gxVmSun[1] * viewSun;
        GLfloat dif[4] = { sunCol[0] * k, sunCol[1] * k, sunCol[2] * k, 1 }, amb[4] = { gxVmAmb[0], gxVmAmb[1], gxVmAmb[2], 1 };
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
        float lv = lp[0] * lp[0] + lp[1] * lp[1] + lp[2] * lp[2];
        if (lp[1] < 0.2f * sqrtf(lv)) { lp[0] *= 0.5f; lp[1] = 0.6f * sqrtf(lv); lp[2] *= 0.5f; }   /* jamais par en dessous */
        glLightfv(GL_LIGHT0, GL_POSITION, lp);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
        glLightfv(GL_LIGHT1, GL_POSITION, lp1);
    }
    glEnable(GL_LIGHTING);
    if (P.meleeKind == MK_ANVIL) draw_anvil_vm();
    else if (P.meleeKind == MK_LANCE) draw_lance_vm();    /* Lance : toujours visible */
    else if (P.meleeKind == MK_FLAIL) draw_flail_vm();    /* Fleau : manche toujours visible, tete au repos */
    else if (P.meleeKind == MK_FIST) {                    /* Poings : gants toujours visibles ; en combo, l'arme s'abaisse */
        draw_fist_vm();
        if (P.fistGuard > 0) glTranslatef(0, -0.24f * P.fistGuard, 0.06f * P.fistGuard);
    } else if (P.meleeKind == MK_SHIELD) {               /* Egide : dessinee apres l'arme (face transparente) */
    } else if (P.meleeT > 0) {                            /* coup d'epee : balayage de droite a gauche */
        float k = clampf(1 - P.meleeT / 0.25f, 0, 1), heat = clampf(vlen(P.vel) / 30, 0, 1);
        glPushMatrix();
        glTranslatef(-0.5f, 0.08f, 0.14f);
        glRotatef(-80 + k * 160, 0, 1, 0);
        glRotatef(-12, 1, 0, 0);
        glRotatef(-35, 0, 0, 1);                          /* lame inclinee */
        glColor3f(0.2f, 0.18f, 0.16f);                    /* poignee + garde */
        box_faces(v3(-0.03f, -0.03f, -0.02f), v3(0.03f, 0.03f, 0.22f));
        box_faces(v3(-0.16f, -0.025f, -0.06f), v3(0.16f, 0.025f, -0.02f));
        glColor3f(0.78f, 0.8f, 0.86f);                    /* lame */
        box_faces(v3(-0.045f, -0.012f, -1.6f), v3(0.045f, 0.012f, -0.06f));
        glDisable(GL_LIGHTING);
        glColor3f(0.4f + heat * 0.6f, 0.8f - heat * 0.3f, 1.0f - heat * 0.8f);   /* le fil chauffe avec la vitesse */
        box_faces(v3(-0.052f, -0.004f, -1.62f), v3(-0.036f, 0.004f, -0.06f));
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
    if (muzzleT > 0) {                                    /* flash du canon */
        float tip = P.weapon == WP_RAIL || P.weapon == WP_SHOTGUN ? -0.64f : P.weapon == WP_KINETIC ? -0.57f : -0.6f;
        if (P.weapon == WP_TESLA) tip = -0.58f;           /* Tesla : entre les fourches */
        float ms2 = (0.12f + 0.25f * (muzzleT / 0.08f)) * (P.weapon == WP_SHOTGUN ? 1.5f : 1.0f);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texDot);
        if (P.weapon == WP_KINETIC) { V3 kc = kin_col(P.kinE); glColor4f(kc.x, kc.y, kc.z, 1); }   /* Elan : couleur du tir */
        else if (P.weapon == WP_SHOTGUN) glColor4f(1, 0.75f, 0.35f, 1);
        else glColor4f(0.5f + 0.5f * ammoDef[P.weapon].r, 0.5f + 0.5f * ammoDef[P.weapon].g, 0.5f + 0.5f * ammoDef[P.weapon].b, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(-ms2, -ms2, tip); glTexCoord2f(1, 0); glVertex3f(ms2, -ms2, tip);
        glTexCoord2f(1, 1); glVertex3f(ms2, ms2, tip); glTexCoord2f(0, 1); glVertex3f(-ms2, ms2, tip);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
    float ready = P.fireCd <= 0 ? 1.0f : 0.35f;
    if (P.weapon == WP_SAW) ready = P.sawCd <= 0 ? 1.0f : 0.35f;
    if (P.weapon == WP_RAIL) {                            /* Rail : canon rond a facettes, frein de bouche, lunette */
        V3 bc = v3(0, 0.005f, 0);                         /* axe du canon */
        glColor3f(0.25f, 0.27f, 0.32f);
        box_faces(v3(-0.05f, -0.06f, -0.42f), v3(0.05f, 0.04f, 0.12f));      /* carcasse */
        box_faces(v3(-0.03f, -0.16f, 0.0f), v3(0.03f, -0.06f, 0.07f));      /* poignee */
        for (int r = 0; r < 2; r++) {                     /* nervures de prise sur la poignee */
            glColor3f(0.12f, 0.12f, 0.15f);
            box_faces(v3(-0.031f, -0.14f + r * 0.035f, 0.005f), v3(0.031f, -0.13f + r * 0.035f, 0.065f));
        }
        glColor3f(0.18f, 0.19f, 0.22f);
        prism_z(bc, -0.64f, -0.40f, 0.024f, 0.032f, 8, 0, 0);               /* canon octogonal */
        glColor3f(0.08f, 0.08f, 0.09f);
        prism_z(bc, -0.637f, -0.634f, 0.014f, 0.012f, 8, 1, 0);             /* fond de l'ame, bouche sombre */
        glColor3f(0.15f, 0.16f, 0.19f);
        prism_z(bc, -0.60f, -0.555f, 0.034f, 0.034f, 8, 1, 1);              /* frein de bouche */
        glColor3f(0.2f, 0.21f, 0.24f);                     /* petites ailettes de refroidissement */
        box_faces(v3(-0.034f, 0.016f, -0.535f), v3(0.034f, 0.026f, -0.515f));
        box_faces(v3(-0.034f, 0.016f, -0.50f), v3(0.034f, 0.026f, -0.48f));
        glColor3f(0.2f, 0.21f, 0.25f);                     /* lunette : tube octogonal sur un petit support */
        box_faces(v3(-0.006f, 0.04f, -0.26f), v3(0.006f, 0.05f, -0.2f));
        prism_z(v3(0, 0.062f, -0.25f), -0.30f, -0.16f, 0.013f, 0.013f, 6, 1, 1);
        glDisable(GL_LIGHTING);
        glColor3f(0.3f * ready, 0.85f * ready, 1.0f * ready);
        prism_z(bc, -0.435f, -0.405f, 0.037f, 0.037f, 8, 1, 1);             /* bague d'energie, lumineuse */
        box_faces(v3(-0.016f, 0.031f, -0.58f), v3(0.016f, 0.036f, -0.40f)); /* rail lumineux sur le dessus */
        prism_z(v3(0, 0.062f, -0.30f), -0.301f, -0.299f, 0.0105f, 0.0105f, 6, 0, 1);   /* reflet de la lentille */
    } else if (P.weapon == WP_KINETIC) {                  /* Elan : corps trapu, trois anneaux = compteur de vitesse */
        float back = P.kinT > 0 ? P.kinT * 0.1f : 0;      /* decharge : recul vers l'arriere */
        glPushMatrix();
        glTranslatef(0, 0, back);
        glColor3f(0.3f, 0.26f, 0.32f);
        box_faces(v3(-0.06f, -0.06f, -0.3f), v3(0.06f, 0.06f, 0.1f));
        glColor3f(0.17f, 0.16f, 0.2f);
        box_faces(v3(-0.028f, -0.028f, -0.55f), v3(0.028f, 0.028f, -0.3f));
        box_faces(v3(-0.03f, -0.17f, -0.02f), v3(0.03f, -0.06f, 0.06f));
        glDisable(GL_LIGHTING);
        float e = kin_vdep() / KIN_SPEED_CAP;
        V3 c = kin_in_window() ? v3(1, 1, 1) : kin_col(e);   /* fenetre du dash-tir : anneaux blancs */
        float lit = P.kinT > 0 ? 0.15f : ready * (0.35f + 0.65f * e);   /* eteints pendant la decharge */
        if (kin_in_window() && P.kinT <= 0) lit = 1;
        for (int r = 0; r < 3; r++) {                     /* volant d'inertie : 3 anneaux autour du canon */
            float z = -0.36f - r * 0.065f, s = 0.05f + 0.006f * r;
            glColor3f(c.x * lit, c.y * lit, c.z * lit);
            box_faces(v3(-s, s - 0.012f, z - 0.018f), v3(s, s, z));
            box_faces(v3(-s, -s, z - 0.018f), v3(s, -s + 0.012f, z));
            box_faces(v3(-s, -s, z - 0.018f), v3(-s + 0.012f, s, z));
            box_faces(v3(s - 0.012f, -s, z - 0.018f), v3(s, s, z));
        }
        glColor3f(c.x * lit * 0.8f, c.y * lit * 0.8f, c.z * lit * 0.8f);
        box_faces(v3(-0.062f, 0.02f, -0.26f), v3(0.062f, 0.032f, 0.0f));
        glPopMatrix();
    } else if (P.weapon == WP_SHOTGUN) {                  /* SHOTGUN : carcasse brune, deux canons, pompe qui recule */
        float k = P.sgPumpMax > 0 && P.sgPumpT > 0 ? 1 - P.sgPumpT / P.sgPumpMax : 1;
        float pz = 0.12f * sinf(PI * clampf((k - 0.35f) / 0.5f, 0, 1));
        glColor3f(0.30f, 0.24f, 0.20f);
        box_faces(v3(-0.06f, -0.06f, -0.30f), v3(0.06f, 0.05f, 0.10f));
        box_faces(v3(-0.03f, -0.17f, 0.0f), v3(0.03f, -0.06f, 0.07f));
        glColor3f(0.42f, 0.44f, 0.48f);                   /* canons, acier */
        box_faces(v3(-0.05f, -0.005f, -0.62f), v3(-0.005f, 0.04f, -0.30f));
        box_faces(v3(0.005f, -0.005f, -0.62f), v3(0.05f, 0.04f, -0.30f));
        glColor3f(0.22f, 0.17f, 0.14f);                   /* pompe */
        box_faces(v3(-0.045f, -0.06f, -0.52f + pz), v3(0.045f, -0.015f, -0.36f + pz));
        glDisable(GL_LIGHTING);
        glColor3f(1.0f * ready, 0.6f * ready, 0.25f * ready);   /* bande cuivre */
        box_faces(v3(-0.04f, 0.05f, -0.28f), v3(0.04f, 0.058f, 0.06f));
        for (int i = 0; i < 6; i++) {                     /* 6 temoins de cartouches ; le suivant se remplit avec la jauge */
            float z = -0.26f + i * 0.05f, fill = i < P.ammo[WP_SHOTGUN] ? 1 : i == P.ammo[WP_SHOTGUN] ? gauge_frac(WP_SHOTGUN) : 0;
            glColor3f(0.15f, 0.12f, 0.1f);
            box_faces(v3(-0.068f, -0.03f, z), v3(-0.06f, 0.006f, z + 0.036f));
            if (fill > 0) {
                glColor3f(1.0f, 0.55f + 0.3f * (fill >= 1), 0.2f);
                box_faces(v3(-0.07f, -0.03f, z + 0.004f), v3(-0.061f, -0.03f + 0.036f * fill, z + 0.032f));
            }
        }
    } else if (P.weapon == WP_SAW) {                      /* Scie : disque a plat dans la main, ou main vide */
        static const float hubVm[3] = { 1.0f, 0.8f, 0.35f };
        Proj *sw = saw_mine();
        if (P.ammo[WP_SAW] > 0) {
            glDisable(GL_LIGHTING);
            draw_saw_disc(v3(0, -0.02f, -0.32f), v3(0, 1, 0), 0.25f, gameTime * 3, v3(0.78f * (0.6f + 0.4f * ready),
                          0.8f * (0.6f + 0.4f * ready), 0.86f * (0.6f + 0.4f * ready)), hubVm);
        } else {
            glColor3f(0.2f, 0.18f, 0.17f);                /* gant */
            box_faces(v3(-0.035f, -0.07f, -0.15f), v3(0.035f, -0.03f, -0.05f));
            if (sw && (sw->sawState == SAW_RETURN || sw->sawState == SAW_RECALL || sw->sawState == SAW_PARRIED)) {
                glDisable(GL_LIGHTING);                   /* lueur cyan : elle revient, c'est le moment du renvoi */
                float pl = 0.6f + 0.4f * sinf(gameTime * 20);
                glColor3f(0.3f * pl, 0.8f * pl, 1.0f * pl);
                box_faces(v3(-0.03f, -0.03f, -0.14f), v3(0.03f, -0.022f, -0.06f));
            }
        }
    } else if (P.weapon == WP_TESLA) {                    /* Tesla */
        draw_tesla_vm();
    } else if (P.weapon == WP_MINE) {                     /* Ventouse */
        draw_mine_vm(ready);
    } else {                                              /* Roquettes : tube a facettes, bouche evasee, tuyere, ogive visible */
        V3 tc = v3(0, 0, 0);                              /* axe du tube */
        int nr = P.ammo[WP_ROCKET];
        glColor3f(0.30f, 0.34f, 0.22f);
        prism_z(tc, -0.50f, 0.04f, 0.058f, 0.058f, 8, 0, 0);                 /* tube */
        glColor3f(0.17f, 0.18f, 0.16f);
        prism_z(tc, -0.60f, -0.50f, 0.078f, 0.066f, 8, 0, 1);                /* bouche evasee */
        prism_z(tc, 0.04f, 0.15f, 0.064f, 0.082f, 8, 1, 0);                  /* tuyere arriere evasee */
        glColor3f(0.05f, 0.05f, 0.06f);
        prism_z(tc, -0.598f, -0.596f, 0.064f, 0.064f, 8, 1, 0);              /* ame sombre a l'avant */
        prism_z(tc, 0.148f, 0.15f, 0.07f, 0.07f, 8, 0, 1);                   /* ame sombre a l'arriere */
        glColor3f(0.22f, 0.24f, 0.18f);                   /* colliers de renfort */
        prism_z(tc, -0.36f, -0.33f, 0.064f, 0.064f, 8, 1, 1);
        prism_z(tc, -0.06f, -0.03f, 0.064f, 0.064f, 8, 1, 1);
        glColor3f(0.16f, 0.16f, 0.15f);                   /* poignee, pontet, poignee avant */
        box_faces(v3(-0.025f, -0.17f, -0.10f), v3(0.025f, -0.05f, -0.03f));
        box_faces(v3(-0.012f, -0.10f, -0.17f), v3(0.012f, -0.088f, -0.10f));
        box_faces(v3(-0.022f, -0.15f, -0.30f), v3(0.022f, -0.05f, -0.25f));
        glColor3f(0.2f, 0.21f, 0.2f);                     /* viseur sur le flanc gauche */
        box_faces(v3(-0.10f, 0.0f, -0.30f), v3(-0.058f, 0.05f, -0.18f));
        glDisable(GL_LIGHTING);
        if (nr > 0) {                                     /* ogive de la roquette chargee, dans la bouche */
            glColor3f(0.9f * ready + 0.1f, 0.42f * ready + 0.08f, 0.1f);
            prism_z(tc, -0.585f, -0.53f, 0.0f, 0.04f, 8, 0, 0);
        }
        glColor3f(1.0f * ready, 0.5f * ready, 0.1f * ready);   /* bandes chaudes autour du tube */
        prism_z(tc, -0.47f, -0.455f, 0.0605f, 0.0605f, 8, 0, 0);
        prism_z(tc, -0.02f, -0.005f, 0.0605f, 0.0605f, 8, 0, 0);
        glColor3f(1.0f, 0.35f + 0.5f * ready, 0.2f);      /* point du viseur */
        box_faces(v3(-0.092f, 0.032f, -0.302f), v3(-0.072f, 0.044f, -0.298f));
        for (int i = 0; i < ammoDef[WP_ROCKET].max; i++) {   /* temoins sur le dessus : une LED par roquette */
            float z = -0.30f + i * 0.032f, on = i < nr ? 1.0f : 0.12f;
            glColor3f(1.0f * on, 0.55f * on, 0.15f * on);
            box_faces(v3(-0.012f, 0.058f, z), v3(0.012f, 0.066f, z + 0.02f));
        }
    }
    if (P.meleeKind == MK_SHIELD) { glEnable(GL_LIGHTING); draw_shield_vm(); }   /* Egide : apres l'arme (transparence) */
}
