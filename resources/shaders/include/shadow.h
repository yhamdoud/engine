#ifndef SHADOW_H
#define SHADOW_H

uint calculate_cascade_index(vec3 pos, float cascade_distances[CASCADE_COUNT])
{
    uint cascade_idx = CASCADE_COUNT - 1;
    for (int i = 0; i < CASCADE_COUNT; i++)
    {
        // Sign change
        if (-pos.z < cascade_distances[i])
        {
            cascade_idx = i;
            break;
        }
    }
    return cascade_idx;

    return 0;
}

#endif