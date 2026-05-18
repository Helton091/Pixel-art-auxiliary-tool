CC = g++
CXXFLAGS = -std=c++17 -I"D:\wangh\all_Trae_file\games\raylib\include" -I.
LDFLAGS = -L"D:\wangh\all_Trae_file\games\raylib\lib" -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32

SOURCES = main.cpp CanvasApp.cpp CanvasDocument.cpp CanvasExporter.cpp WindowsFileDialog.cpp
TARGET = CanvasTool.exe

all:
	$(CC) $(SOURCES) $(CXXFLAGS) -o $(TARGET) $(LDFLAGS)

clean:
	del $(TARGET)
