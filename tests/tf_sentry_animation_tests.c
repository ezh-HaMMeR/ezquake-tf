#include "tf_sentry_animation.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
int main(void)
{
    char names[TF_SENTRY_SPIN_FRAMES][56] = {{0}};
    int l, i, a, b; float f;
    for (l=0;l<3;l++) for(i=0;i<24;i++) snprintf(names[10+l*24+i],16,"tfspin%d_%02d",l+1,i);
    assert(TF_SentrySpinFramesValid(names[0],82,56));
    {
        char rig[13][56] = {{0}};
        for (l=0;l<3;l++) snprintf(rig[10+l],16,"tfrig%d_x",l+1);
        assert(TF_SentryRigFramesValid(rig[0],13,56));
        assert(!TF_SentryRigFramesValid(rig[0],12,56));
        rig[12][15]=1;assert(!TF_SentryRigFramesValid(rig[0],13,56));
    }
    assert(!TF_SentrySpinFramesValid(names[0],81,56));
    names[81][15]=1;assert(!TF_SentrySpinFramesValid(names[0],82,56));names[81][15]=0;
    for (l=0;l<3;l++) {
        int base=10+l*24;
        assert(TF_SentrySpinNextFrame(base+23)==base);
        assert(TF_SentrySpinNextFrame(base)==base+1);
        assert(TF_SentrySpinPoses(l*3+1,l*3,.5f,.005,&a,&b,&f));
        assert(a==base && b==base+1 && fabs(f-.24)<.00001);
        assert(TF_SentrySpinPoses(l*3,l*3+1,1.f,.49,&a,&b,&f));
        assert(a==base+23 && b==base && f>0 && f<1);
        assert(!TF_SentrySpinPoses(l*3,l*3+1,1.25f,.5,&a,&b,&f));
        assert(!TF_SentrySpinPoses(l*3,l*3+1,-1,.5,&a,&b,&f));
        assert(!TF_SentrySpinPoses(l*3,l*3,0,.5,&a,&b,&f));
        assert(!TF_SentrySpinPoses(l*3,((l+1)%3)*3+1,.5f,.5,&a,&b,&f));
        /* Full time sweep includes loop seams and all level boundaries. */
        for(i=0;i<10000;i++) {
            assert(TF_SentrySpinPoses(l*3+1,l*3,0,i/997.0,&a,&b,&f));
            assert(a>=base && a<base+24 && b>=base && b<base+24 && f>=0 && f<1);
        }
    }
    assert(!TF_SentrySpinPoses(9,8,0,1,&a,&b,&f));
    assert(TF_SentrySpinNextFrame(9)==-1 && TF_SentrySpinNextFrame(82)==-1);
    assert(!TF_SentrySpinPoses(-1,0,0,1,&a,&b,&f));
    assert(!TF_SentrySpinPoses(1,0,0,-1,&a,&b,&f));
    puts("TF sentry animation tests passed");return 0;
}
