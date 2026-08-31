#ifndef TRACERAY_COMP
#define TRACERAY_COMP

#include "bindingLayout.h"
#include "functions.glsl"

void ClosestHit(Ray r)
{
  uint rayFlags = gl_RayFlagsNoneEXT;//gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;  // gl_RayFlagsNoneEXT
  prd.hitT      = INFINITY;

  // Initializes a ray query object but does not start traversal
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery,     //
                        topLevelAS,   // acceleration structure
                        rayFlags,     // rayFlags
                        0xFF,         // cullMask
                        r.origin,     // ray origin
                        0.0,          // ray min range
                        r.direction,  // ray direction
                        INFINITY);    // ray max range

  while(rayQueryProceedEXT(rayQuery))
  {
    if(rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
    {
        rayQueryConfirmIntersectionEXT(rayQuery);  // The hit was opaque
        break;
    }
  }
  bool hit = (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT);
  if(hit)
  {
    prd.hitT                = rayQueryGetIntersectionTEXT(rayQuery, true);
    prd.primitiveID         = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    prd.instanceID          = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
    prd.instanceCustomIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
    prd.baryCoord           = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
    prd.lastPosition        = rayQueryGetIntersectionObjectRayOriginEXT(rayQuery,true);  
    prd.objectToWorld       = rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true);
    prd.worldToObject       = rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true);
  }
}

//-----------------------------------------------------------------------
// Shoot a ray an return the information of the closest hit, in the
// PtPayload structure (PRD)
//
void ClosestHitT(Ray r)
{
  uint rayFlags = gl_RayFlagsNoneEXT;//gl_RayFlagsTerminateOnFirstHitEXT;  // gl_RayFlagsNoneEXT
  prd.hitT      = INFINITY;

  // Initializes a ray query object but does not start traversal
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery,     //
                        topLevelAS,   // acceleration structure
                        rayFlags,     // rayFlags
                        0xFF,         // cullMask
                        r.origin,     // ray origin
                        0.0,          // ray min range
                        r.direction,  // ray direction
                        INFINITY);    // ray max range

  while(rayQueryProceedEXT(rayQuery))
  {
    if(rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
    {
        rayQueryConfirmIntersectionEXT(rayQuery);  // The hit was opaque
        break;
    }
  }
  bool hit = (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);
  if(hit)
  {
    prd.hitT                = rayQueryGetIntersectionTEXT(rayQuery, true);
    prd.primitiveID         = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    prd.instanceID          = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
    prd.instanceCustomIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
    prd.baryCoord           = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
    prd.lastPosition        = rayQueryGetIntersectionObjectRayOriginEXT(rayQuery,true);  
    prd.objectToWorld       = rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true);
    prd.worldToObject       = rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true);
  }
}

// //-----------------------------------------------------------------------
// // Shoot a ray an return the information of the closest hit, in the
// // PtPayload structure (PRD)
// //
// void ClosestHit(Ray r)
// {
//   uint rayFlags = gl_RayFlagsNoneEXT;//gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;//
//   prd.hitT      = INFINITY;

//   // Initializes a ray query object but does not start traversal
//   rayQueryEXT rayQuery;
//   rayQueryInitializeEXT(rayQuery,     //
//                         topLevelAS,   // acceleration structure
//                         rayFlags,     // rayFlags
//                         0xFF,         // cullMask
//                         r.origin,     // ray origin
//                         0.001,          // ray min range
//                         r.direction,  // ray direction
//                         INFINITY);    // ray max range

//   int num = 0;
//   bool isFirst = true;
//   float hitT0 = -1;
//   float hitT = -1;
//   // Start traversal, and loop over all ray-scene intersections. When this finishes,
//   // rayQuery stores a "committed" intersection, the closest intersection (if any).
//   while(rayQueryProceedEXT(rayQuery))
//   {
//     if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
//     {
//         // Determine if an opaque triangle hit occurred
//         rayQueryConfirmIntersectionEXT(rayQuery);
//         break;
//     }
//     else if (rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionAABBEXT)
//     {
//         // // Get the t-value of the intersection (if there's no intersection, this will
//         // // be tMax = 10000.0). "true" says "get the committed intersection."
//         prd.instanceCustomIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, false);
//         prd.primitiveID         = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, false);
//         int loc = instanceLinks[prd.instanceCustomIndex].voxelIdOffset + prd.primitiveID;
//         AllAabb aabb = aabbs[loc];

//         hitT0 = hitAabb(aabb, r);
//         // if(hitT0 > 0)
//         // {
//         //   if(isFirst)
//         //   {
//         //     hitT = hitT0;
//         //     isFirst = false;
//         //     continue;
//         //   }
//         //   if(hitT0 < hitT)
//         //   {
//         //     hitT = hitT0;
//         //   }
//         // }     

//         // hitT = prd.primitiveID; 
//         hitT = hitT0;
//         rayQueryGenerateIntersectionEXT(rayQuery, hitT);
//         break;
//     }
//   }
  
//   // prd.hitT = num;
//   prd.hitT                = rayQueryGetIntersectionTEXT(rayQuery, true); 
//   // prd.hitT         = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
//   // // prd.instanceID          = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
//   // prd.instanceCustomIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);

//   // prd.lastPosition        = rayQueryGetIntersectionObjectRayOriginEXT(rayQuery,true);  
//   // prd.objectToWorld       = rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true);
//   // prd.worldToObject       = rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true);
// }


void ClosestHit(Ray r,float minLength)
{
  uint rayFlags = gl_RayFlagsNoneEXT | gl_RayFlagsOpaqueEXT;
  // uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT;
  prd.hitT      = INFINITY;

  // Initializes a ray query object but does not start traversal
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery,     //
                        topLevelAS,   // acceleration structure
                        rayFlags,     // rayFlags
                        0xFF,         // cullMask
                        r.origin,     // ray origin
                        minLength,          // ray min range
                        r.direction,  // ray direction
                        INFINITY);    // ray max range


  // rayQueryConfirmIntersectionEXT(rayQuery);
  while(rayQueryProceedEXT(rayQuery))
  {
    if(rayQueryGetIntersectionTypeEXT(rayQuery, false) == gl_RayQueryCandidateIntersectionTriangleEXT)
    {
        rayQueryConfirmIntersectionEXT(rayQuery);  // The hit was opaque
        break;
    }
  }
  bool hit = (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);
  if(hit)
  {
    prd.hitT                = rayQueryGetIntersectionTEXT(rayQuery, true);
    prd.primitiveID         = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    prd.instanceID          = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
    prd.instanceCustomIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
    prd.baryCoord           = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
    prd.lastPosition        = rayQueryGetIntersectionObjectRayOriginEXT(rayQuery,true);  
    prd.objectToWorld       = rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true);
    prd.worldToObject       = rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true);
  }
}

//-----------------------------------------------------------------------
// Shadow ray - return true if a ray hits anything
//
bool AnyHit(Ray r, float maxDist)
{
  shadow_payload.isHit = true;      // Asume hit, will be set to false if hit nothing (miss shader)
  shadow_payload.seed  = prd.seed;  // don't care for the update - but won't affect the rahit shader
  uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsCullBackFacingTrianglesEXT;

  // Initializes a ray query object but does not start traversal
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery,     //
                        topLevelAS,   // acceleration structure
                        rayFlags,     // rayFlags
                        0xFF,         // cullMask
                        r.origin,     // ray origin
                        0.0,          // ray min range
                        r.direction,  // ray direction
                        maxDist);     // ray max range

  // Start traversal: return false if traversal is complete
  // add to ray contribution from next event estimation
  return (rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionNoneEXT);
}




#endif 