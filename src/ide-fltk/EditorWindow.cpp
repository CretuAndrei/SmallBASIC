// -*- c-file-style: "java" -*-
// $Id: EditorWindow.cpp,v 1.18 2004-12-18 06:15:50 zeeb90au Exp $
//
// Based on test/editor.cxx - A simple text editor program for the Fast 
// Light Tool Kit (FLTK). This program is described in Chapter 4 of the FLTK 
// Programmer's Guide.
// Copyright 1998-2003 by Bill Spitzak and others.
//
// Copyright(C) 2001-2004 Chris Warren-Smith. Gawler, South Australia
// cwarrens@twpo.com.au
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <io.h>

#include <fltk/ask.h>
#include <fltk/events.h>
#include <fltk/file_chooser.h>
#include <fltk/Flags.h>
#include <fltk/FileChooser.h>
#include <fltk/MenuBar.h>
#include <fltk/Input.h>
#include <fltk/Button.h>
#include <fltk/ReturnButton.h>
#include <fltk/TextBuffer.h>
#include <fltk/TextEditor.h>

#include "EditorWindow.h"
#include "MainWindow.h"
#include "kwp.cxx"

using namespace fltk;

TextDisplay::StyleTableEntry styletable[] = { // Style table
    { BLACK,            COURIER, 14 }, // A - Plain
    { color(0,128,0),   COURIER, 14 }, // B - Ccomments
    { color(0,0,192),   COURIER, 14 }, // C - Strings
    { color(128,0,0),   COURIER_BOLD, 14 }, // D - code_keywords
    { color(128,128,0), COURIER_BOLD, 14 }, // E - code_functions
    { color(0,128,128), COURIER_BOLD, 14 }  // F - code_procedures
};

TextBuffer *stylebuf = 0;
TextBuffer *textbuf = 0;

// 'compare_keywords()' - Compare two keywords
int compare_keywords(const void *a, const void *b) {
    return (strcmpi(*((const char **)a), *((const char **)b)));
}

// 'style_parse()' - Parse text and produce style data.
void style_parse(const char *text, char *style, int length) {
    char current = 'A';
    int  col = 0;
    int  last = 0;
    char  buf[255];
    char *bufptr;
    const char *temp;

    for (;length > 0; length--, text++) {
        if (current == 'B') { 
            current = 'A';
        }
        
        if (current == 'A') {
            // check for directives, comments, strings, and keywords
            if ((*text == '#' && !isdigit(text[1])) || 
                *text == '\'' || strncmpi(text, "rem", 3) == 0) {
                // basic comment
                current = 'B';
                for (; length > 0 && *text != '\n'; length--, text ++) {
                    *style++ = 'B';
                }
                if (length == 0) {
                    break;
                }
            } else if (strncmp(text, "\\\"", 2) == 0) {
                // quoted quote
                *style++ = current;
                *style++ = current;
                text ++;
                length--;
                col += 2;
                continue;
            } else if (*text == '\"') {
                current = 'C';
            } else if (!last) {
                // might be a keyword
                temp = text;
                bufptr = buf;
                while (*temp != 0 && *temp != ' ' && *temp != '\n' && 
                       *temp != '(' && *temp != ')' && *temp != '=' &&
                       bufptr < (buf + sizeof(buf) - 1)) {
                    *bufptr++ = *temp++;
                }
                
                *bufptr = '\0';
                bufptr = buf;

                if (bsearch(&bufptr, code_keywords,
                            sizeof(code_keywords) / sizeof(code_keywords[0]),
                            sizeof(code_keywords[0]), compare_keywords)) {
                   
                    while (text < temp) {
                        *style++ = 'D';
                        text++;
                        length--;
                        col++;
                    }
                    text--;
                    length++;
                    last = 1;
                    continue;
                } else if (bsearch(&bufptr, code_functions,
                                   sizeof(code_functions) / sizeof(code_functions[0]),
                                   sizeof(code_functions[0]), compare_keywords)) {
                    while (text < temp) {
                        *style++ = 'E';
                        text++;
                        length--;
                        col++;
                    }

                    text--;
                    length++;
                    last = 1;
                    continue;
                } else if (bsearch(&bufptr, code_procedures,
                                   sizeof(code_procedures) / sizeof(code_procedures[0]),
                                   sizeof(code_procedures[0]), compare_keywords)) {
                    while (text < temp) {
                        *style++ = 'F';
                        text++;
                        length--;
                        col++;
                    }

                    text--;
                    length++;
                    last = 1;
                    continue;
                }
            }
        } else if (current == 'C') {
            // continuing in string
            if (strncmp(text, "\\\"", 2) == 0) {
                // quoted end quote
                *style++ = current;
                *style++ = current;
                text++;
                length--;
                col += 2;
                continue;
            } else if (*text == '\"') {
                // End quote
                *style++ = current;
                col++;
                current = 'A';
                continue;
            }
        }

        // copy style info
        if (current == 'A' && (*text == '{' || *text == '}')) {
            *style++ = 'E';
        } else {
            *style++ = current;
        }
        col++;
        last = isalnum(*text) || *text == '.';

        if (*text == '\n') {
            col = 0; // reset column 
            current = 'A'; // basic lines do not continue
        }
    }
}

// 'style_init()' - Initialize the style buffer
void style_init(void) {
    char *style = new char[textbuf->length() + 1];
    const char *text = textbuf->text();

    memset(style, 'A', textbuf->length());
    style[textbuf->length()] = '\0';

    if (!stylebuf) {
        stylebuf = new TextBuffer(textbuf->length());
    }

    style_parse(text, style, textbuf->length());
    stylebuf->text(style);
    delete[] style;
}

// 'style_unfinished_cb()' - Update unfinished styles.
void style_unfinished_cb() {}

char* get_style_range(int start, int end) {
    const char* s = stylebuf->text_range(start, end);
    char *style = new char[strlen(s) + 1];
    strcpy(style, s);
    return style;
}

// 'style_update()' - Update the style buffer
void style_update(int pos,        // I - Position of update
                  int nInserted,  // I - Number of inserted chars
                  int nDeleted,   // I - Number of deleted chars
                  int /*nRestyled*/,  // I - Number of restyled chars
                  const char * /*deletedText*/,// I - Text that was deleted
                  void *cbArg) {   // I - Callback data

    int start;              // Start of text
    int end;                // End of text
    char  last;             // Last style on line
    const char *text;       // Text data
    char *style;            // Text data

    // if this is just a selection change, just unselect the style buffer
    if (nInserted == 0 && nDeleted == 0) {
        stylebuf->unselect();
        return;
    }

    // track changes in the text buffer
    if (nInserted > 0) { 
        // insert characters into the style buffer
        char *stylex = new char[nInserted + 1];
        memset(stylex, 'A', nInserted);
        stylex[nInserted] = '\0';
        stylebuf->replace(pos, pos + nDeleted, stylex);
        delete[] stylex;
    } else {
        // just delete characters in the style buffer
        stylebuf->remove(pos, pos + nDeleted);
    }

    // Select the area that was just updated to avoid unnecessary callbacks
    stylebuf->select(pos, pos + nInserted - nDeleted);

    // re-parse the changed region; we do this by parsing from the
    // beginning of the line of the changed region to the end of
    // the line of the changed region  Then we check the last
    // style character and keep updating if we have a multi-line
    // comment character
    start = textbuf->line_start(pos);
    end   = textbuf->line_end(pos + nInserted);
    text  = textbuf->text_range(start, end);
    style = get_style_range(start, end);
    last  = style[end - start - 1];

    //  printf("start = %d, end = %d, text = \"%s\", style = \"%s\"...\n",
    //         start, end, text, style);

    style_parse(text, style, end - start);

    //  printf("new style = \"%s\"...\n", style);

    stylebuf->replace(start, end, style);
    ((TextEditor *)cbArg)->redisplay_range(start, end);

    if (last != style[end - start - 1]) {
        // the last character on the line changed styles, 
        // so reparse the remainder of the buffer
        delete[] style;

        end   = textbuf->length();
        text  = textbuf->text_range(start, end);
        style = get_style_range(start, end);

        style_parse(text, style, end - start);
        stylebuf->replace(start, end, style);
        ((TextEditor *)cbArg)->redisplay_range(start, end);
    }

    delete[] style;
}

//--CodeEditor------------------------------------------------------------------

struct CodeEditor : public TextEditor {
    CodeEditor(int x, int y, int w, int h) : TextEditor(x, y, w, h) {
        readonly = false;
        undoBuff = 0;
        curBuff = 0;
    }
    
    int handle(int e) {
        if (readonly && (e == KEY || e == PASTE)) {
            return 0;
        }
        
        int r = TextEditor::handle(e);
        if (e == KEYUP || e == RELEASE) {
            int row, col;
            position_to_linecol(mCursorPos, &row, &col);
            if (row < 9999 && col < 9999) {
                setRowCol(row, col+1);
            }
        }
        return r;
    }
    
    void saveUndo() {
        if (undoBuff) {
            free(undoBuff);
        }
        undoBuff = curBuff;
        curBuff = strdup(buffer()->text());
        oldCursorPos = mCursorPos;
    }       
    
    void undo() {
        if (undoBuff) {
            buffer()->text(undoBuff);
            free(undoBuff);
            undoBuff = 0;
            mCursorPos = oldCursorPos;
        }
    }
    
    void gotoLine(int line) {
        int numLines = buffer()->count_lines(0, buffer()->length());
        if (line < 1) {
            line = 1;
        } else if (line > numLines) {
            line = numLines;
        }
        
        scroll(line-(mNVisibleLines/2), 0);
        insert_position(buffer()->skip_lines(0, line-1));
    }

    void getRowCol(int *row, int *col) {
        position_to_linecol(mCursorPos, row, col);
    }

    void getSelStartRowCol(int *row, int *col) {
        int start = buffer()->primary_selection()->start();
        int end = buffer()->primary_selection()->end();
        if (start == end) {
            *row = -1;
            *col = -1;
        } else {
            position_to_linecol(start, row, col);
        }
    }

    void getSelEndRowCol(int *row, int *col) {
        int start = buffer()->primary_selection()->start();
        int end = buffer()->primary_selection()->end();
        if (start == end) {
            *row = -1;
            *col = -1;
        } else {
            position_to_linecol(end, row, col);
        }
    }

    int position() {
        return mCursorPos;
    }

    int oldCursorPos;
    bool readonly;
    char* undoBuff;
    char* curBuff;
};

//--EditorWindow----------------------------------------------------------------

EditorWindow::EditorWindow(int x, int y, int w, int h) : 
    Group(x, y, w, h) {

    replaceDlg = new Window(300, 105, "Replace");
    replaceDlg->begin();
    replaceFind = new Input(80, 10, 210, 25, "Find:");
    replaceFind->align(ALIGN_LEFT);
    replaceWith = new Input(80, 40, 210, 25, "Replace:");
    replaceWith->align(ALIGN_LEFT);
    Button* replaceAll = new Button(10, 70, 90, 25, "Replace All");
    replaceAll->callback((Callback *)replall_cb, this);
    ReturnButton* replaceNext = 
        new ReturnButton(105, 70, 120, 25, "Replace Next");
    replaceNext->callback((Callback *)replace2_cb, this);
    Button* replaceCancel = new Button(230, 70, 60, 25, "Cancel");
    replaceCancel->callback((Callback *)replcan_cb, this);
    replaceDlg->end();
    replaceDlg->set_non_modal();

    search[0] = 0;
    filename[0] = 0; 
    dirty = false;
    loading = false;
    textbuf = new TextBuffer();
    style_init();

    begin();
    editor = new CodeEditor(0, 0, w, h);
    editor->buffer(textbuf);
    editor->highlight_data(stylebuf, styletable,
                           sizeof(styletable) / sizeof(styletable[0]),
                           'A', style_unfinished_cb, 0);
    editor->textfont(COURIER);
    editor->box(THIN_DOWN_BOX);
    editor->cursor_style(TextDisplay::BLOCK_CURSOR);
    editor->selection_color(fltk::color(192,192,192));
    end();
    resizable(editor);

    textbuf->add_modify_callback(style_update, editor);
    textbuf->add_modify_callback(changed_cb, this);
}

EditorWindow::~EditorWindow() {
    delete replaceDlg;
    textbuf->remove_modify_callback(style_update, editor);
    textbuf->remove_modify_callback(changed_cb, this);
}

bool EditorWindow::readonly() {
    return ((CodeEditor*)editor)->readonly;
}

void EditorWindow::readonly(bool is_readonly) {
    editor->cursor_style(is_readonly ? TextDisplay::DIM_CURSOR : 
                         TextDisplay::BLOCK_CURSOR);
    ((CodeEditor*)editor)->readonly = is_readonly;
}

void EditorWindow::doChange(int inserted, int deleted) {
    if ((inserted || deleted) && !loading) {
        dirty = 1;
        ((CodeEditor*)editor)->saveUndo();
    }

    if (loading) {
        editor->show_insert_position();
    }
    
    setModified(dirty);
}

bool EditorWindow::checkSave(bool discard) {
    if (!dirty) {
        return true; // continue next operation
    }

    const char* msg = "The current file has not been saved.\n"
        "Would you like to save it now?";
    int r = discard ? 
        choice(msg, "Save", "Discard", "Cancel") :
        choice(msg, "Save", "Cancel", 0);

    if (r == 0) {
        saveFile(); // Save the file
        return !dirty;
    }
    return (discard && r==1);
}

void EditorWindow::loadFile(const char *newfile, int ipos) {
    loading = true;
    int insert = (ipos != -1);
    dirty = insert;
    if (!insert) {
        strcpy(filename, "");
    }

    int r;
    if (!insert) {
        r = textbuf->loadfile(newfile);
    } else {
        r = textbuf->insertfile(newfile, ipos);
    }

    if (r) {
        alert("Error reading from file \'%s\':\n%s.", newfile, strerror(errno));
    } else {
        if (!insert) {
            strcpy(filename, newfile);
        }
    }
    loading = false;
    textbuf->call_modify_callbacks();
    setTitle(filename);
    addHistory(filename);
}

void EditorWindow::doSaveFile(const char *newfile, bool updateUI) {
    char basfile[PATH_MAX];
    strcpy(basfile, newfile);
    int len = strlen(basfile);
    if (stricmp(basfile+len-4, ".bas") != 0) {
        strcat(basfile, ".bas");
    }

    if (textbuf->savefile(basfile)) {
        alert("Error writing to file \'%s\':\n%s.", basfile, strerror(errno));
        return;
    }

    if (updateUI) {
        dirty = 0;
        strcpy(filename, basfile);
        textbuf->call_modify_callbacks();
        setTitle(basfile);
    }
}

void EditorWindow::find() {
    const char *val = input("Search String:", search);
    if (val != NULL) {
        // user entered a string - go find it
        strcpy(search, val);
        findNext();
    }
}

void EditorWindow::findNext() {
    if (search[0] == '\0') {
        // search string is blank; get a new one
        find();
        return;
    }

    int pos = editor->insert_position();
    int found = textbuf->search_forward(pos, search, &pos);
    if (found) {
        // Found a match; select and update the position
        textbuf->select(pos, pos+strlen(search));
        editor->insert_position(pos+strlen(search));
        editor->show_insert_position();
    } else {
        alert("No occurrences of \'%s\' found!", search);
    }
}

void EditorWindow::newFile() {
    if (readonly()) {
        return;
    }

    if (!checkSave(true)) {
        return;
    }

    filename[0] = '\0';
    textbuf->select(0, textbuf->length());
    textbuf->remove_selection();
    dirty = 0;
    textbuf->call_modify_callbacks();
    setTitle(0);
}

void EditorWindow::openFile() {
    if (readonly()) {
        return;
    }

    if (!checkSave(true)) {
        return;
    }

    char *newfile = file_chooser("Open File?", "*.bas", filename);
    if (newfile != NULL) {
        loadFile(newfile, -1);
    }
}

void EditorWindow::insertFile() {
    if (readonly()) {
        return;
    }

    char *newfile = file_chooser("Insert File?", "*.bas", filename);
    if (newfile != NULL) {
        loadFile(newfile, editor->insert_position());
    }
}

void EditorWindow::replaceNext() {
    if (readonly()) {
        return;
    }

    const char *find = replaceFind->value();
    const char *replace = replaceWith->value();

    if (find[0] == '\0') {
        // search string is blank; get a new one
        replaceDlg->show();
        return;
    }

    replaceDlg->hide();

    int pos = editor->insert_position();
    int found = textbuf->search_forward(pos, find, &pos);

    if (found) {
        // found a match; update the position and replace text
        textbuf->select(pos, pos+strlen(find));
        textbuf->remove_selection();
        textbuf->insert(pos, replace);
        textbuf->select(pos, pos+strlen(replace));
        editor->insert_position(pos+strlen(replace));
        editor->show_insert_position();
    } else {
        alert("No occurrences of \'%s\' found!", find);
    }
}

void EditorWindow::replaceAll() {
    if (readonly()) {
        return;
    }

    const char *find = replaceFind->value();
    const char *replace = replaceWith->value();

    find = replaceFind->value();
    if (find[0] == '\0') {
        // search string is blank; get a new one
        replaceDlg->show();
        return;
    }

    replaceDlg->hide();
    editor->insert_position(0);
    int times = 0;

    // loop through the whole string
    for (int found = 1; found;) {
        int pos = editor->insert_position();
        found = textbuf->search_forward(pos, find, &pos);

        if (found) {
            // found a match; update the position and replace text
            textbuf->select(pos, pos+strlen(find));
            textbuf->remove_selection();
            textbuf->insert(pos, replace);
            editor->insert_position(pos+strlen(replace));
            editor->show_insert_position();
            times++;
        }
    }

    if (times) {
        message("Replaced %d occurrences.", times);
    } else {
        alert("No occurrences of \'%s\' found!", find);
    }
}

void EditorWindow::cancelReplace() {
    replaceDlg->hide();
}

void EditorWindow::saveFile() {
    if (filename[0] == '\0') {
        // no filename - get one!
        saveFileAs();
        return;
    } else {
        doSaveFile(filename, true);
    }
}

void EditorWindow::saveFileAs() {
    const char* msg = 
        "%s\n\nFile already exists.\nDo you want to replace it?";
    char* newfile = file_chooser("Save File As?", "*.bas", filename);
    if (newfile != NULL) {
        if (access(newfile, 0) == 0 && ask(msg, newfile) == 0) {
            return;
        }
        doSaveFile(newfile, true);
    }
}

void EditorWindow::doDelete() {
    textbuf->remove_selection();
}

void EditorWindow::undo() {
    ((CodeEditor*)editor)->undo();
}

void EditorWindow::gotoLine(int line) {
    ((CodeEditor*)editor)->gotoLine(line);
}

void EditorWindow::getRowCol(int* row, int* col) {
    return ((CodeEditor*)editor)->getRowCol(row, col);
}

int EditorWindow::position() {
    return ((CodeEditor*)editor)->position();
}

void EditorWindow::position(int pos) {
    editor->insert_position(pos);
    editor->show_insert_position();
}

void EditorWindow::getSelStartRowCol(int *row, int *col) {
    return ((CodeEditor*)editor)->getSelStartRowCol(row, col);
}

void EditorWindow::getSelEndRowCol(int *row, int *col) {
    return ((CodeEditor*)editor)->getSelEndRowCol(row, col);
}

//--EndOfFile-------------------------------------------------------------------
