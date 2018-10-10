
TARGET   = ../Release/Plugins/Plugin_Flite_plus_hts_engine.so

SOURCES  = Flite_plus_hts_engine.cpp \
           Flite_plus_hts_engine_Manager.cpp \
           Flite_plus_hts_engine_Thread.cpp \
           Plugin_Flite_plus_hts_engine.cpp

OBJECTS  = $(SOURCES:.cpp=.o)

LDADD    = ../Library_MMDAgent/lib/MMDAgent.a \
           ../Library_MMDFiles/lib/MMDFiles.a \
           ../Library_GLFW/lib/GLFW.a \
           ../Library_GLee/lib/GLee.a \
           ../Library_Bullet_Physics/lib/Bullet_Physics.a \
           ../Library_libpng/lib/libpng.a \
           ../Library_FreeType/lib/FreeType.a \
           ../Library_JPEG/lib/JPEG.a \
           ../Library_zlib/lib/zlib.a \
           ../Library_hts_engine_API/lib/hts_engine_API.a \
           ../Library_Flite_plus_hts_engine/lib/Flite_plus_hts_engine.a \
           ../Library_PortAudio/lib/PortAudio.a

CXX      = gcc
AR       = ar
CXXFLAGS = -Wall -O3 -fomit-frame-pointer -fPIC \
           -shared \
           -DMMDAGENT
INCLUDE  = -I ../Library_Bullet_Physics/include \
           -I ../Library_GLee/include \
           -I ../Library_GLFW/include \
           -I ../Library_MMDFiles/include \
           -I ../Library_MMDAgent/include \
           -I ../Library_hts_engine_API/include \
           -I ../Library_Flite_plus_hts_engine/include \
           -I ../Library_PortAudio/include

all: $(TARGET)

$(TARGET): $(OBJECTS) $(LDADD)
	$(CXX) $(CXXFLAGS) $(OBJECTS) $(LDADD) -o $(TARGET) \
	-lGLU -lGL -lX11 -lasound -ldl -lrt -lm -lpthread

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(<:.cpp=.o) -c $<

clean:
	rm -f $(OBJECTS) $(TARGET)
