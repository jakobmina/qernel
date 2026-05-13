#include <stdio.h>
#include <stdlib.h>
#include "metriplectic.h"

int main() {
    GenerativeActor actor = {0, 0, 0.5, 0.5, 0};
    TorsionObservables obs = {0.5, 0.2, 0.3, 1.0};
    
    printf("[MANDATO] CLI de Actor Generativo\n");
    printf("step,x,y,energy\n");
    for (int i = 0; i < 50; i++) {
        double On = golden_operator(i * 0.05);
        actor = update_actor(actor, obs, On);
        printf("%d,%.4f,%.4f,%.4f\n", i, actor.x, actor.y, actor.energy);
    }
    return 0;
}
