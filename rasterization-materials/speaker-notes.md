# 03 triangles
probably give some context for barycentric coordintes.

basically each vertex on the triangle corresponds to a standard basis
in that dimension, and points elsewhere are a linear blend of each.

# 04 bezier
probably give some context for lerp / linear interpolation.

the function linearly blends two values, accepting a `t` parameter
from 0 to 1 that says how much of each value to use. for example,
lerping with `t=0` spits back `a`, and `t=1` spits back `b`.

```c
float lerp(float a, float b, float t)
{
	return t * b + (1 - t) * a;
}
```
