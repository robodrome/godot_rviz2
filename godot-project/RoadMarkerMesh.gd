extends MeshInstance

var vector_map = MarkerArray.new()

func cross_product(a, b):
	return Vector3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x)

func _ready():
	vector_map.subscribe("/map/vector_map_marker", true)

func _process(_delta):
	if !vector_map.is_new():
		return
	mesh.clear_surfaces()

	var arr = []
	arr.resize(Mesh.ARRAY_MAX)
	var verts = PoolVector3Array()
#	var uvs = PoolVector2Array()
	var normals = PoolVector3Array()
#	var indices = PoolIntArray()
#	var colors = PoolColorArray()

	verts.append_array(vector_map.get_triangle_marker("right_lane_bound"))
	verts.append_array(vector_map.get_triangle_marker("left_lane_bound"))
	verts.append_array(vector_map.get_triangle_marker("pedestrian_marking"))
	verts.append_array(vector_map.get_triangle_marker("stop_lines"))

	for i in verts.size():
		normals.append(Vector3(0,1,0))
#		if(i % 3 ==2):
#			normals.append(cross_product(verts[i] - verts[i-2], verts[i-1] - verts[i-2]).normalized())
#			normals.append(cross_product(verts[i] - verts[i-2], verts[i-1] - verts[i-2]).normalized())
#			normals.append(cross_product(verts[i] - verts[i-2], verts[i-1] - verts[i-2]).normalized())

	arr[Mesh.ARRAY_VERTEX] = verts
	arr[Mesh.ARRAY_NORMAL] = normals
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arr)
	vector_map.set_old()
