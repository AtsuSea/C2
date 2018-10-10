/* ----------------------------------------------------------------- */
/*           The Toolkit for Building Voice Interaction Systems      */
/*           "MMDAgent" developed by MMDAgent Project Team           */
/*           http://www.mmdagent.jp/                                 */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2009-2016  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAgent project team nor the names of  */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

#import "MMDViewController.h"
#import "MMDAgent.h"
#import <OpenGLES/ES2/glext.h>
#import <AVFoundation/AVFoundation.h>

#define TAPDURATION 0.3

extern MMDAgent *mmdagent;
extern bool enable;

char *openURLString = NULL;    /* open URL string given before launch by URL scheme */
float startCameraDistance;     /* camera distance at the beginning of pinch gesture */
int startLine;
int tapStatus = 0;             /* tap status, 1 to wait panning gesture for TAPDURATION sec after a tap  */
int panMode = 0;               /* panning mode, 0 = rotate, 1 = translate */

/* define this to enable high-resolution rendering on retina display */
#define RETINA_HIGHRES

@implementation MMDViewController

- (void)initView 
{
   GLKView *view = (GLKView *)self.view;

   /* init view */
   view.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
   [EAGLContext setCurrentContext:view.context];
   view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
   view.drawableStencilFormat = GLKViewDrawableStencilFormat8;
   view.drawableMultisample = GLKViewDrawableMultisample4X;
#ifdef RETINA_HIGHRES
   view.contentScaleFactor = [UIScreen mainScreen].scale;
#endif /* RETINA_HIGHRES */

   self.preferredFramesPerSecond = 60;

   view.userInteractionEnabled = true;
   view.multipleTouchEnabled = true;
}

- (void)viewDidLoad
{
   [super viewDidLoad];
   
   [self initView];
   glfwInitForIOS();

   /* set up audio session of this application */
   NSError *error = nil;
   BOOL success;

   /* initialize audio session assigned to this application */
   AVAudioSession *session = [AVAudioSession sharedInstance];
   /* set up audio session category for PlayAndRecord with options */
   /*
      AVAudioSessionCategoryOptionMixWithOthers
         allow sound mixing whyen other sound application is active
         (invalid when background mode is not allowed)
      AVAudioSessionCategoryOptionDuckOthers;
         lower other volume (start/stop session between sound start)
   */
   /* success = [session setCategory: AVAudioSessionCategoryPlayAndRecord withOptions: AVAudioSessionCategoryOptionMixWithOthers error: &error]; */
   success = [session setCategory: AVAudioSessionCategoryPlayAndRecord error: &error];
   if (!success) {
     /* category setting error */
   }
   /* specialize the audio session category by specifying modes */
   /*
      AVAudioSessionModeVideoChat
        for PlayAndRecord, enable voice-specific processing (AGC etc.)
        audio routes are optimized for video chat: enable bluetooth and speaker
      AVAudioSessionModeMeasurement;
        disable sound signal processing
   */
   success = [session setMode: AVAudioSessionModeVideoChat error: &error];
   if (!success) {
     /* mode setting error */
   }

   /* activate audio session */
   success = [session setActive: YES error: &error];
   if (!success) {
     /* activation error */
   }

   /* set up gesture recognizer */
   /* tap */
   UITapGestureRecognizer *tapRecognizer = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tapped:)];
   tapRecognizer.numberOfTapsRequired = 1;
   [self.view addGestureRecognizer:tapRecognizer];
   /* pinch */
   UIPinchGestureRecognizer *pinchRecognizer = [[UIPinchGestureRecognizer alloc] initWithTarget:self action:@selector(pinched:)];
   [self.view addGestureRecognizer:pinchRecognizer];
   /* pan */
   UIPanGestureRecognizer *panRecognizer = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panned:)];
   [self.view addGestureRecognizer:panRecognizer];
   /* long press */
   UILongPressGestureRecognizer *longPressRecognizer = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longPressed:)];
   [self.view addGestureRecognizer:longPressRecognizer];
   /* edge pan */
   UIScreenEdgePanGestureRecognizer *edgePanRecognizer = [[UIScreenEdgePanGestureRecognizer alloc] initWithTarget:self action:@selector(rightEdgePanned:)];
   [edgePanRecognizer setEdges:UIRectEdgeRight];
   [self.view addGestureRecognizer:edgePanRecognizer];
   [panRecognizer requireGestureRecognizerToFail:edgePanRecognizer];
   UIScreenEdgePanGestureRecognizer *edgePanRecognizer2 = [[UIScreenEdgePanGestureRecognizer alloc] initWithTarget:self action:@selector(leftEdgePanned:)];
   [edgePanRecognizer2 setEdges:UIRectEdgeLeft];
   [self.view addGestureRecognizer:edgePanRecognizer2];
   [panRecognizer requireGestureRecognizerToFail:edgePanRecognizer2];
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
   if (enable == true) {
      /* process main loop */
      mmdagent->updateAndRender();
      /* check if reset was ordered while setup process */
      if (mmdagent->getResetFlag() == true) {
         mmdagent->restart("");
         [self createMenu];
      }
   } else {
      /* for the first time, launch contents */
      /* get argc, argv, and append open url string if given at startup */
      NSArray *args = [[NSProcessInfo processInfo] arguments];
      char **argv;
      int argc = (int)[args count];
      if (openURLString != NULL)
         argc++;
      argv = (char **)malloc(argc * sizeof(char*));
      for (int i = 0; i < (int)[args count]; i++)
         argv[i] = MMDAgent_strdup([(NSString *)args[i] UTF8String]);
      if (openURLString != NULL)
         argv[argc - 1] = openURLString;

      // setup MMDAgent
      mmdagent = new MMDAgent();
      if(mmdagent->setup(argc, argv, "") == false) {
         delete mmdagent;
         return;
      }
      [self createMenu];

      /* set view size */
      CGRect rect = [[UIScreen mainScreen] bounds];
#ifdef RETINA_HIGHRES
      mmdagent->procWindowSizeMessage(rect.size.width * [UIScreen mainScreen].scale, rect.size.height * [UIScreen mainScreen].scale);
#else
      mmdagent->procWindowSizeMessage(rect.size.width, rect.size.height);
#endif /* RETINA_HIGHRES */

      /* check if reset was ordered while setup */
      if (mmdagent->getResetFlag() == true) {
         mmdagent->restart("");
         [self createMenu];
      }

      /* free */
      for (int i = 0; i < argc; i++)
         free(argv[i]);
      free(argv);

      enable = true;
   }
}

/* menu handler */
static void menuHandler(int id, int item, void *data)
{
   MMDAgent *mmdagent;
   Menu *menu;
   int len;
   char buff[MMDAGENT_MAXBUFLEN];
   char buff2[MMDAGENT_MAXBUFLEN];

   if (data == NULL)
      return;
   mmdagent = (MMDAgent *)data;
   menu = mmdagent->getMenu();
   if (menu == NULL)
      return;

   if (menu->find("[Open]") == id) {
      if (item == 0) {
         mmdagent->setResetFlag("");
      } else {
         sprintf(buff, "%s/Documents/mmda%d-config.txt", getenv("HOME"), item);
         FILE *fp = MMDAgent_fopen(buff, "rb");
         if (fp) {
            fgets(buff2, MMDAGENT_MAXBUFLEN, fp);
            fgets(buff2, MMDAGENT_MAXBUFLEN, fp);
            len = MMDAgent_strlen(buff2);
            buff2[len -1] = '\0';
            fclose(fp);
            sprintf(buff, "%s/Documents/mmda%d/%s", getenv("HOME"), item, buff2);
            mmdagent->setResetFlag(buff);
         }
      }
   } else if (menu->find("[System]") == id) {
      switch (item) {
         case 0: /* logWindow */
            mmdagent->procDisplayLogMessage(); break;
         case 1: /* FPS */
            mmdagent->procInfoStringMessage(); break;
         case 2: /* Shadow */
            mmdagent->procShadowMessage(); break;
         case 3: /* FileBrowser */
            if (mmdagent->getFileBrowser()) {
               if (mmdagent->getFileBrowser()->isShowing()) {
                  mmdagent->getFileBrowser()->hide();
               } else {
                  mmdagent->getFileBrowser()->show();
               }
               mmdagent->getMenu()->hide();
            }
            break;
      }
   } else if (menu->find("[Develop]"), id) {
      switch (item) {
         case 0: /* Bone */
            mmdagent->procDisplayBoneMessage(); break;
         case 1: /* Wire */
            mmdagent->procDisplayWireMessage(); break;
         case 2: /* RigidBody */
            mmdagent->procDisplayRigidBodyMessage(); break;
         case 3: /* Physics */
            mmdagent->procPhysicsMessage(); break;
      }
   }
}

- (void)createMenu
{
   int id;
   Menu *menu;
   int i;
   char buff[MMDAGENT_MAXBUFLEN];
   char buff2[MMDAGENT_MAXBUFLEN];
   int len;

   if (mmdagent == NULL)
      return;

   menu = mmdagent->getMenu();
   if (menu == NULL)
      return;

   id = menu->add("[Open]", MENUPRIORITY_SYSTEM, menuHandler, mmdagent);
   menu->setItem(id, 0, "<Built-in>", NULL, NULL);
   for (i = 0; i < MMDAGENT_MAXCONTENTNUM; i++) {
      sprintf(buff, "%s/Documents/mmda%d-config.txt", getenv("HOME"), i + 1);
      FILE *fp = MMDAgent_fopen(buff, "rb");
      if (fp) {
         fgets(buff2, MMDAGENT_MAXBUFLEN, fp);
         len = MMDAgent_strlen(buff2);
         buff2[len - 1] = '\0';
         menu->setItem(id, i + 1, buff2, NULL, NULL);
         fclose(fp);
      }
   }
   id = menu->add("[System]", MENUPRIORITY_SYSTEM, menuHandler, mmdagent);
   menu->setItem(id, 0, "Log ON/OFF", NULL, NULL);
   menu->setItem(id, 1, "FPS ON/OFF", NULL, NULL);
   menu->setItem(id, 2, "Shadow ON/OFF", NULL, NULL);
   menu->setItem(id, 3, "File Viewer", NULL, NULL);
   id = menu->add("[Develop]", MENUPRIORITY_DEVELOP, NULL, NULL);
   menu->setItem(id, 0, "Disp Bone", NULL, NULL);
   menu->setItem(id, 1, "Disp Wire", NULL, NULL);
   menu->setItem(id, 2, "Disp RigidBody", NULL, NULL);
   menu->setItem(id, 3, "Physics ON/OFF", NULL, NULL);
}

- (void)tapped:(UITapGestureRecognizer *)sender
{
   CGRect rect = [[UIScreen mainScreen] bounds];
   CGPoint location = [sender locationInView:self.view];
   Menu *m;
   FileBrowser *f;
   Prompt *p;

   if (mmdagent == NULL) {
      m = NULL;
      f = NULL;
      p = NULL;
   } else {
      m = mmdagent->getMenu();
      f = mmdagent->getFileBrowser();
      p = mmdagent->getPrompt();
   }

   if (m && m->isShowing()) {
      /* menu is active: execute item if tapped, else hide */
      if (m->isPointed(location.x, location.y, rect.size.width, rect.size.height))
         m->execByTap(location.x, location.y, rect.size.width, rect.size.height);
      else
         m->hide();
   } else if (f && f->isShowing()) {
      /* file browser is active: execute item if tapped, else hide */
      if (f->isPointed(location.x, location.y, rect.size.width, rect.size.height))
         f->execByTap(location.x, location.y, rect.size.width, rect.size.height);
      else
         f->hide();
   } else if (p && p->isShowing()) {
      /* prompt is active: execute item if tapped, else hide */
      if (p->isPointed(location.x, location.y, rect.size.width, rect.size.height))
         p->execByTap(location.x, location.y, rect.size.width, rect.size.height);
      else
         p->cancel();
   } else {
      /* set flag and call tapExecute after TAPDURATION sec to distinguish single tap and tap-pan */
      tapStatus = 1;
      [self performSelector:@selector(tapExecute:) withObject:[NSValue valueWithCGPoint:location] afterDelay:TAPDURATION];
   }
}

- (void)tapExecute:(NSValue *)locationValue
{
   CGPoint location;
   [locationValue getValue:&location];
   if (tapStatus == 1) {
      tapStatus = 0;
      /* no panning after a tap, so execute tap */
       mmdagent->procMouseLeftButtonDownMessage(location.x, location.y, false, false);
       mmdagent->procMouseLeftButtonUpMessage();
   }
}

CGPoint sp;       /* pan starting point */
int direction_x;  /* detected panning direction for X */
int direction_y;  /* detected panning direction for Y */

- (void) panned:(UIPanGestureRecognizer*)sender
{
   CGRect rect = [[UIScreen mainScreen] bounds];
   CGPoint location = [sender locationInView:self.view];
   CGPoint p;
   float r1, r2;
   float vx, vy;
   Menu *m;
   FileBrowser *f;

   if (mmdagent == NULL) {
      m = NULL;
      f = NULL;
   } else {
      m = mmdagent->getMenu();
      f = mmdagent->getFileBrowser();
   }

   p = [sender translationInView:self.view];
   vx = location.x - sp.x;
   vy = location.y - sp.y;

   switch(sender.state) {
      case UIGestureRecognizerStateBegan:
         /* store pan starting point */
         sp = location;
         if (tapStatus == 1) {
            /* panning gesture just after a tap: translation mode */
            tapStatus = 0;
            panMode = 1;
         } else {
            /* panning gesture without tap: rotation mode */
            panMode = 0;
         }
         direction_x = 0;
         direction_y = 0;
         break;
      case UIGestureRecognizerStateChanged:
         if (direction_x == 0) {
            if (vx < -4.0f) {
               /* start left pan */
               direction_x = -1;
               /* if menu is active and pointed, forward menu */
               if (m && m->isPointed(sp.x, sp.y, rect.size.width, rect.size.height) && m->isShowing())
                  m->forward();
            } else if (vx > 4.0f) {
               /* start right pan */
               direction_x = 1;
               /* if menu is active and pointed, backward menu */
               if (m && m->isPointed(sp.x, sp.y, rect.size.width, rect.size.height) && m->isShowing())
                  m->backward();
            }
         }
         if (direction_y == 0) {
            if (vy < -1.0f)
               direction_y = -1;
            else if (vy > 1.0f)
               direction_y = 1;
         }
         if (m && m->isPointed(sp.x, sp.y, rect.size.width, rect.size.height) && m->isShowing()) {
            /* if menu is active and pointed, update animation while panning */
            if (direction_x == 1){
               mmdagent->getMenu()->forceBackwardAnimationRate((location.x - sp.x) / rect.size.width);
            } else if (direction_x == -1) {
               mmdagent->getMenu()->forceForwardAnimationRate((sp.x - location.x)/ rect.size.width);
            }
         } else if (f && f->isPointed(sp.x, sp.y, rect.size.width, rect.size.height) && f->isShowing()) {
            /* if file browser is active and pointed, scroll for vertical pan and update animation for horizontal pan */
            if (fabs(vx) < fabs(vy)) {
               f->scroll(-direction_y);
            } else if (fabs(vx / rect.size.width) > 0.1f){
               f->setBackSlideAnimationRate(vx / rect.size.width);
            }
         } else {
            if (panMode == 1) {
               /* translation */
               CGRect rect = [[UIScreen mainScreen] bounds];
               r1 = p.x / rect.size.width;
               r2 = p.y / rect.size.height;
               if (mmdagent->getRenderer() != NULL)
                  mmdagent->getRenderer()->translateWithView(r1 * 1.3f, -r2 * 1.3f, 0.0f);
            } else {
               /* rotation */
               r1 = p.x; r2 = p.y;
               if (r1 > 32767) r1 -= 65536;
               if (r1 < -32768) r1 += 65536;
               if (r2 > 32767) r2 -= 65536;
               if (r2 < -32768) r2 += 65536;
               if (mmdagent->getRenderer() != NULL)
                  mmdagent->getRenderer()->rotate(r2 * 0.5f, r1 * 0.5f, 0.0f);
            }
         }
         break;
      case UIGestureRecognizerStateEnded:
      case UIGestureRecognizerStateCancelled:
         if (m && m->isPointed(sp.x, sp.y, rect.size.width, rect.size.height) && m->isShowing()) {
            /* if menu is active and pointed, release animation and finish move forward/backward */
            if (direction_x == 1){
               mmdagent->getMenu()->forceBackwardAnimationRate(-1.0f);
            } else if (direction_x == -1) {
               mmdagent->getMenu()->forceForwardAnimationRate(-1.0f);
            }
         } else if (f && f->isPointed(sp.x, sp.y, rect.size.width, rect.size.height)) {
            /* if file browser is active and pointed, release animation and go back (right pan) or hide (left pan) */
            f->setBackSlideAnimationRate(0.0f);
            if (fabs(vx) > fabs(vy)) {
               if (vx / rect.size.width > 0.2f) {
                  f->back();
               } else if (vx / rect.size.width  < -0.2f) {
                  f->hide();
               }
            }
         }
         break;
      default:
         break;
   }
   [sender setTranslation:CGPointZero inView:self.view];
}

- (void) pinched:(UIPinchGestureRecognizer*)sender
{
   FileBrowser *f;
   float r;

   if (mmdagent == NULL)
      f = NULL;
   else
      f = mmdagent->getFileBrowser();

   if (f && f->isShowing()) {
      /* zoom in/out for file browser */
      if (sender.state == UIGestureRecognizerStateBegan)
         startLine = f->getLines();
      f->setLines((int)((float)startLine / sender.scale));
      return;
   }

   /* zoom in/out view */
   if (sender.state == UIGestureRecognizerStateBegan)
      startCameraDistance = mmdagent->getRenderer()->getDistance();

   if (fabs(sender.scale) < 0.001f) {
      r = 100.0f;
   } else {
      r = 1.0f / sender.scale;
   }
   if (mmdagent->getRenderer() != NULL)
      mmdagent->getRenderer()->setDistance(startCameraDistance * r);
}

- (void) longPressed:(UILongPressGestureRecognizer*)sender
{
   /* toggle log message */
   if (sender.state == UIGestureRecognizerStateBegan)
      mmdagent->procDisplayLogMessage();
}

- (void) rightEdgePanned:(UIScreenEdgePanGestureRecognizer*)sender
{
   Menu *m;

   /* show menu or go forward menu */
   switch(sender.state) {
      case UIGestureRecognizerStateBegan:
         if (mmdagent == NULL)
            return;
         m = mmdagent->getMenu();
         if (m == NULL)
            return;

         if (m->isShowing() == false) {
            m->show();
         } else {
            m->forward();
         }
         break;
      case UIGestureRecognizerStateChanged:
      case UIGestureRecognizerStateEnded:
      case UIGestureRecognizerStateCancelled:
         break;
      default:
         break;
   }
   [sender setTranslation:CGPointZero inView:self.view];
}

- (void)leftEdgePanned:(UIScreenEdgePanGestureRecognizer*)sender
{
   FileBrowser *f;

   /* show file browser, only when menu is not active */
   switch(sender.state) {
      case UIGestureRecognizerStateBegan:
         if (mmdagent == NULL)
            return;
         f = mmdagent->getFileBrowser();
         if (f == NULL)
            return;

         if (mmdagent->getMenu() && mmdagent->getMenu()->isShowing())
            return;

         if (f->isShowing() == false) {
            f->show();
         }
         break;
      case UIGestureRecognizerStateChanged:
      case UIGestureRecognizerStateEnded:
      case UIGestureRecognizerStateCancelled:
         break;
      default:
         break;
   }
   [sender setTranslation:CGPointZero inView:self.view];
}

- (void)willAnimateRotationToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
{
   CGRect rect = [[UIScreen mainScreen] bounds];
#ifdef RETINA_HIGHRES
   mmdagent->procWindowSizeMessage(rect.size.width * [UIScreen mainScreen].scale, rect.size.height * [UIScreen mainScreen].scale);
#else
   mmdagent->procWindowSizeMessage(rect.size.width, rect.size.height);
#endif /* RETINA_HIGHRES */
}

@end
