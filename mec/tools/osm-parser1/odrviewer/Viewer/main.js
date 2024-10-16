/* globals */
var ModuleOpenDrive = null;
var OpenDriveMap = null;
var tilecache = null;
var refline_lines = null;
var road_network_mesh = null;
var roadmarks_mesh = null;
var lane_outline_lines = null;
var roadmark_outline_lines = null;
var ground_grid = null;
var disposable_objs = [];
var mouse = new THREE.Vector2();
var spotlight_info = document.getElementById('spotlight_info');
var INTERSECTED_LANE_ID = 0xffffffff;
var INTERSECTED_ROADMARK_ID = 0xffffffff;
var spotlight_paused = false;

const COLORS = {
    road : 1.0,
    roadmark : 1.0,
    road_object : 0.9,
    lane_outline : 0xae52d4,
    roadmark_outline : 0xffffff,
    ref_line : 0x69f0ae,
    background : 0x444444,
    lane_highlight : 0x0288d1,
    roadmark_highlight : 0xff0000,
};

/* event listeners */
window.addEventListener('resize', onWindowResize, false);
window.addEventListener('mousemove', onDocumentMouseMove, false);
window.addEventListener('dblclick', onDocumentMouseDbClick, false);

/* notifactions */
const notyf = new Notyf({
    duration : 3000,
    position : { x : 'left', y : 'bottom' },
    types : [ { type : 'info', background : '#607d8b', icon : false } ]
});

/* THREEJS renderer */
const renderer = new THREE.WebGLRenderer({ antialias : true, sortObjects : false });
renderer.shadowMap.enabled = true;
renderer.setSize(window.innerWidth, window.innerHeight);
document.getElementById('ThreeJS').appendChild(renderer.domElement);

/* THREEJS globals */
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 100000);
camera.up.set(0, 0, 1); /* Coordinate system with Z pointing up */
const controls = new THREE.MapControls(camera, renderer.domElement);
controls.addEventListener('start', () => { spotlight_paused = true; controls.autoRotate = false; });
controls.addEventListener('end', () => { spotlight_paused = false; });
controls.autoRotate = true;

/* THREEJS lights */
const light = new THREE.DirectionalLight(0xffffff, 1.0);
scene.add(light);
scene.add(light.target);

/* THREEJS auxiliary globals */
const lane_picking_scene = new THREE.Scene();
lane_picking_scene.background = new THREE.Color(0xffffff);
const roadmark_picking_scene = new THREE.Scene();
roadmark_picking_scene.background = new THREE.Color(0xffffff);
const xyz_scene = new THREE.Scene();
xyz_scene.background = new THREE.Color(0xffffff);
const st_scene = new THREE.Scene();
st_scene.background = new THREE.Color(0xffffff);
const lane_picking_texture = new THREE.WebGLRenderTarget(1, 1, { type : THREE.FloatType });
const roadmark_picking_texture = new THREE.WebGLRenderTarget(1, 1, { type : THREE.FloatType });
const xyz_texture = new THREE.WebGLRenderTarget(1, 1, { type : THREE.FloatType });
const st_texture = new THREE.WebGLRenderTarget(1, 1, { type : THREE.FloatType });

/* THREEJS materials */
const idVertexShader = document.getElementById('idVertexShader').textContent;
const idFragmentShader = document.getElementById('idFragmentShader').textContent;
const xyzVertexShader = document.getElementById('xyzVertexShader').textContent;
const xyzFragmentShader = document.getElementById('xyzFragmentShader').textContent;
const stVertexShader = document.getElementById('stVertexShader').textContent;
const stFragmentShader = document.getElementById('stFragmentShader').textContent;

const refline_material = new THREE.LineBasicMaterial({
    color : COLORS.ref_line,
});
const road_network_material = new THREE.MeshPhongMaterial({
    vertexColors : THREE.VertexColors,
    wireframe : PARAMS.wireframe,
    shininess : 20.0,
    transparent : true,
    opacity : 0.4
});
const lane_outlines_material = new THREE.LineBasicMaterial({
    color : COLORS.lane_outline,
});
const roadmark_outlines_material = new THREE.LineBasicMaterial({
    color : COLORS.roadmark_outline,
});
const id_material = new THREE.ShaderMaterial({
    vertexShader : idVertexShader,
    fragmentShader : idFragmentShader,
});
const xyz_material = new THREE.ShaderMaterial({
    vertexShader : xyzVertexShader,
    fragmentShader : xyzFragmentShader,
});
const st_material = new THREE.ShaderMaterial({
    vertexShader : stVertexShader,
    fragmentShader : stFragmentShader,
});
const roadmarks_material = new THREE.MeshBasicMaterial({
    vertexColors : THREE.VertexColors,
});
const road_objects_material = new THREE.MeshBasicMaterial({
    vertexColors : THREE.VertexColors,
    side : THREE.DoubleSide,
    wireframe : true,
});

/* load WASM + odr map */
libOpenDrive().then(Module => {
    ModuleOpenDrive = Module;
    // fetch("./data.xodr").then((file_data) => {
    //     file_data.text().then((file_text) => {
    //         loadFile(file_text, false);
    //     });
    // });
});

function onFileSelect(file)
{
    let file_reader = new FileReader();
    file_reader.onload = () => { loadFile(file_reader.result, true); };
    file_reader.readAsText(file);
}
var tmpblockid = 113;
var blockids = [113];
var blockidindex = 0;

function loadBuf()
{
    OpenDriveMap = null;
    loadFile(null, null);
}

function loadFile(file_text, clear_map)
{
    if(file_text) 
    {
        if (clear_map)
            ModuleOpenDrive['FS_unlink']('./data.xodr');
        ModuleOpenDrive['FS_createDataFile'](".", "data.xodr", file_text, true, true);

        if (OpenDriveMap)
            OpenDriveMap.delete();

        odr_map_config = {
            with_lateralProfile : PARAMS.lateralProfile,
            with_laneHeight : PARAMS.laneHeight,
            with_road_objects : false,
            center_map : true,
            abs_z_for_for_local_road_obj_outline : true
        };
        OpenDriveMap = new ModuleOpenDrive.OpenDriveMap("./data.xodr", odr_map_config);

        loadOdrMap(clear_map);
    }
    else {
        odr_map_config = {
            with_lateralProfile : PARAMS.lateralProfile,
            with_laneHeight : PARAMS.laneHeight,
            with_road_objects : false,
            center_map : true,
            abs_z_for_for_local_road_obj_outline : true
        };
        
        // if (blockids[blockidindex] == tmpblockid) {
        tilecache = new ModuleOpenDrive.tilecache();
        bindmap();
        // }
        var reqBlockId = blockids[blockidindex];
        // var dependBlocks = getDepBlockList(1);
        // getBlockInfo(blockids, dependBlocks);
        // modifyBlockids(dependBlocks);
    }
}

function modifyBlockids(dependBlocks)
{
    // blockidindex++;
    if (dependBlocks) {
        for(var i=0; i < dependBlocks.length; i++) {
            if (blockids.indexOf(dependBlocks[i]) == -1) {
                blockids.push(dependBlocks[i]);
            }
        }
    }
}

function bindmap() {
    var oReq = new XMLHttpRequest();
    if (window.ActiveXObject)
    {
        oReq = new ActiveXObject("Microsoft.XMLHTTP");
    }
    else if (window.XMLHttpRequest)
    {
        oReq = new XMLHttpRequest();
    }
    oReq.open("POST", "http://222.71.128.20:55559/hdmap/v1/info/get", true);
    oReq.setRequestHeader("Content-Type","application/json");
    oReq.responseType = "arraybuffer";

    oReq.onload = function (oEvent) {
        var arrayBuffer = oReq.response; // Note: not oReq.responseText
        tilecache.bind_map(arrayBuffer);

        // var p2d = [{"x":-1294, "y":-4565}, {"x":-1274, "y":-4565}, {"x":-1274, "y":-4545}, {"x":-1294, "y":-4545}];
        // var respBlkids = tilecache.query_unloaded_blocks(p2d);
        // console.log(respBlkids);
        // respBlkids = tilecache.filter_loaded_blocks(respBlkids, true);
        // console.log(respBlkids);
        var ret = tilecache.get_map_georef();
        console.log(ret);
        var resutl = tilecache.get_mapoffset();
        console.log(resutl);
        // var mapid = tilecache.get_mapid();
        // console.log(mapid);
        var lightBlks = [113];
        getBlockInfo(lightBlks, []);
        
        
    };
    oReq.send('{"accessToken": "testaccesstoken"}');
}

function getBlockInfo(reqBlockids,depBlockids)
{
    var depBlockidsStr = "";
    for(var i = 0; i < depBlockids.length;i++) {
        depBlockidsStr += depBlockids[i];
        if(i < depBlockids.length -1) {
            depBlockidsStr += ",";
        }
    }
    var reqBlockidsStr = "";
    for(var i = 0; i < reqBlockids.length;i++) {
        reqBlockidsStr += reqBlockids[i];
        if(i < reqBlockids.length -1) {
            reqBlockidsStr += ",";
        }
    }
    var oReq = new XMLHttpRequest();
    if (window.ActiveXObject)
    {
        oReq = new ActiveXObject("Microsoft.XMLHTTP");
    }
    else if (window.XMLHttpRequest)
    {
        oReq = new XMLHttpRequest();
    }
    oReq.open("POST", "http://222.71.128.20:55559/hdmap/v1/render/block/info/get", true);
    oReq.setRequestHeader("Content-Type","application/json");
    oReq.responseType = "arraybuffer";

    oReq.onload = function (oEvent) {
        var arrayBuffer = oReq.response; // Note: not oReq.responseText
        tilecache.add_tile(arrayBuffer);
        // tilecache.get_mesh(224);
        // tilecache.get_mesh(225);
        // tilecache.get_mesh(208);
        // var lightList = tilecache.get_singlelight_blocks(reqBlockids);
        // console.log(lightList);
        // var center = tilecache.get_junctioncenter_byappr("3_11_7");
        // console.log(center);
        loadOdrMap(null);
    };
    oReq.send('{"accessToken": "74aa4a9a11cbd8abec646e54e2eb6bc4", "requestBlocks": [113], "dependBlocks": [], "uuid": "B783EDA7-9818-A347-A098-A0858861F7F6"}');
}

function getDepBlockList(reqBlockId) 
{
    var depBlockList = [];
    $.ajax({
        url: "http://222.71.128.20:55559/hdmap/v1/render/blkdeps/get",
        type: "POST",
        contentType: "application/json",
        async: false,
        data: '{"accessToken": "74aa4a9a11cbd8abec646e54e2eb6bc4","blockids": [' + reqBlockId + '],"uuid": "B783EDA7-9818-A347-A098-A0858861F7F6"}',
        dataType:"json",
        success: function(result) {
            // console.log(result);
            if(result["code"] == 0) {
                for (var i = 0;i<result["data"]["dependBlocks"].length;i++) {
                    depBlockList.push(result["data"]["dependBlocks"][i]);
                }
            }
        }
    })
    return depBlockList;
}

function reloadOdrMap()
{
    if (OpenDriveMap)
        OpenDriveMap.delete();

    OpenDriveMap = new ModuleOpenDrive.OpenDriveMap("./data.xodr", PARAMS.lateralProfile, PARAMS.laneHeight, true, false);
    loadOdrMap(true, false);
}

function loadOdrMap(clear_map = true, fit_view = true)
{
    const t0 = performance.now();
    if (clear_map) {
        road_network_mesh.userData.odr_road_network_mesh.delete();
        scene.remove(road_network_mesh, roadmarks_mesh, road_objects_mesh, refline_lines, lane_outline_lines, roadmark_outline_lines, ground_grid);
        lane_picking_scene.remove(...lane_picking_scene.children);
        roadmark_picking_scene.remove(...roadmark_picking_scene.children);
        xyz_scene.remove(...xyz_scene.children);
        st_scene.remove(...st_scene.children);
        for (let obj of disposable_objs)
            obj.dispose();
    }

    /* reflines */
    reflines_geom = new THREE.BufferGeometry();
    odr_refline_segments = null;
    if (OpenDriveMap) {
        odr_refline_segments = ModuleOpenDrive.get_refline_segments(OpenDriveMap, parseFloat(PARAMS.resolution));

        reflines_geom.setAttribute('position', new THREE.Float32BufferAttribute(getStdVecEntries(odr_refline_segments.vertices).flat(), 3));
        reflines_geom.setIndex(getStdVecEntries(odr_refline_segments.indices, true));
        refline_lines = new THREE.LineSegments(reflines_geom, refline_material);
        refline_lines.renderOrder = 10;
        refline_lines.visible = PARAMS.ref_line;
        refline_lines.matrixAutoUpdate = false;
        disposable_objs.push(reflines_geom);
        scene.add(refline_lines);
    }

    /* road network geometry */
    odr_road_network_mesh = null;
    if(OpenDriveMap) {
        odr_road_network_mesh = ModuleOpenDrive.get_road_network_mesh(OpenDriveMap, parseFloat(PARAMS.resolution));
    }
    if(tilecache) {
        var testbool = true;
        var meshid = blockids[blockidindex];
        odr_road_network_mesh = tilecache.get_mesh(meshid);
        blockidindex++;

        odr_refline_segments = odr_road_network_mesh.refline;
        reflines_geom.setAttribute('position', new THREE.Float32BufferAttribute(getStdVecEntries(odr_refline_segments.vertices).flat(), 3));
        reflines_geom.setIndex(getStdVecEntries(odr_refline_segments.indices, true));
        refline_lines = new THREE.LineSegments(reflines_geom, refline_material);
        refline_lines.renderOrder = 10;
        refline_lines.visible = PARAMS.ref_line;
        refline_lines.matrixAutoUpdate = false;
        disposable_objs.push(reflines_geom);
        scene.add(refline_lines);
    }
    const odr_lanes_mesh = odr_road_network_mesh.lanes_mesh;
    const road_network_geom = get_geometry(odr_lanes_mesh);
    road_network_geom.attributes.color.array.fill(COLORS.road);
    
    let lane_index = odr_lanes_mesh.lane_start_type;
    const map_keys_vec = lane_index.keys();
    for (let idx = 0; idx < map_keys_vec.size(); idx++)
        console.log("lane vetex " + map_keys_vec.get(idx) +" type " +lane_index.get(map_keys_vec.get(idx)));

    for (const [vert_start_idx, _] of getStdMapEntries(odr_lanes_mesh.lane_start_indices)) {
        const vert_idx_interval = odr_lanes_mesh.get_idx_interval_lane(vert_start_idx);
        const vert_count = vert_idx_interval[1] - vert_idx_interval[0];
        const vert_start_idx_encoded = encodeUInt32(vert_start_idx);
        const attr_arr = new Float32Array(vert_count * 4);
        for (let i = 0; i < vert_count; i++)
            attr_arr.set(vert_start_idx_encoded, i * 4);
        road_network_geom.attributes.id.array.set(attr_arr, vert_idx_interval[0] * 4);
    }
    disposable_objs.push(road_network_geom);

    /* road network mesh */
    road_network_mesh = new THREE.Mesh(road_network_geom, road_network_material);
    road_network_mesh.renderOrder = 0;
    road_network_mesh.userData = { odr_road_network_mesh };
    road_network_mesh.matrixAutoUpdate = false;
    road_network_mesh.visible = !(PARAMS.view_mode == 'Outlines');
    scene.add(road_network_mesh);

    /* picking road network mesh */
    const lane_picking_mesh = new THREE.Mesh(road_network_geom, id_material);
    lane_picking_mesh.matrixAutoUpdate = false;
    lane_picking_scene.add(lane_picking_mesh);

    /* xyz coords road network mesh */
    const xyz_mesh = new THREE.Mesh(road_network_geom, xyz_material);
    xyz_mesh.matrixAutoUpdate = false;
    xyz_scene.add(xyz_mesh);

    /* st coords road network mesh */
    const st_mesh = new THREE.Mesh(road_network_geom, st_material);
    st_mesh.matrixAutoUpdate = false;
    st_scene.add(st_mesh);

    /* roadmarks geometry */
    const odr_roadmarks_mesh = odr_road_network_mesh.roadmarks_mesh;
    const roadmarks_geom = get_geometry(odr_roadmarks_mesh);
    roadmarks_geom.attributes.color.array.fill(COLORS.roadmark);
    for (const [vert_start_idx, _] of getStdMapEntries(odr_roadmarks_mesh.roadmark_type_start_indices)) {
        const vert_idx_interval = odr_roadmarks_mesh.get_idx_interval_roadmark(vert_start_idx);
        const vert_count = vert_idx_interval[1] - vert_idx_interval[0];
        const vert_start_idx_encoded = encodeUInt32(vert_start_idx);
        const attr_arr = new Float32Array(vert_count * 4);
        for (let i = 0; i < vert_count; i++)
            attr_arr.set(vert_start_idx_encoded, i * 4);
        roadmarks_geom.attributes.id.array.set(attr_arr, vert_idx_interval[0] * 4);
    }
    disposable_objs.push(roadmarks_geom);

    /* roadmarks mesh */
    roadmarks_mesh = new THREE.Mesh(roadmarks_geom, roadmarks_material);
    roadmarks_mesh.matrixAutoUpdate = false;
    roadmarks_mesh.visible = !(PARAMS.view_mode == 'Outlines') && PARAMS.roadmarks;
    scene.add(roadmarks_mesh);

    /* picking roadmarks mesh */
    const roadmark_picking_mesh = new THREE.Mesh(roadmarks_geom, id_material);
    roadmark_picking_mesh.matrixAutoUpdate = false;
    roadmark_picking_scene.add(roadmark_picking_mesh);

    /* road objects geometry */
    const odr_road_objects_mesh = odr_road_network_mesh.road_objects_mesh;
    const road_objects_geom = get_geometry(odr_road_objects_mesh);
    road_objects_geom.attributes.color.array.fill(COLORS.road_object);
    for (const [vert_start_idx, _] of getStdMapEntries(odr_road_objects_mesh.road_object_start_indices)) {
        const vert_idx_interval = odr_roadmarks_mesh.get_idx_interval_roadmark(vert_start_idx);
        const vert_count = vert_idx_interval[1] - vert_idx_interval[0];
        const vert_start_idx_encoded = encodeUInt32(vert_start_idx);
        const attr_arr = new Float32Array(vert_count * 4);
        for (let i = 0; i < vert_count; i++)
            attr_arr.set(vert_start_idx_encoded, i * 4);
        roadmarks_geom.attributes.id.array.set(attr_arr, vert_idx_interval[0] * 4);
    }
    disposable_objs.push(road_objects_geom);

    /* road objects mesh */
    road_objects_mesh = new THREE.Mesh(road_objects_geom, road_objects_material);
    road_objects_mesh.matrixAutoUpdate = false;
    scene.add(road_objects_mesh);

    /* lane outline */
    const lane_outlines_geom = new THREE.BufferGeometry();
    lane_outlines_geom.setAttribute('position', road_network_geom.attributes.position);
    lane_outlines_geom.setIndex(getStdVecEntries(odr_lanes_mesh.get_lane_outline_indices(), true));
    lane_outline_lines = new THREE.LineSegments(lane_outlines_geom, lane_outlines_material);
    lane_outline_lines.renderOrder = 9;
    disposable_objs.push(lane_outlines_geom);
    scene.add(lane_outline_lines);

    /* roadmark outline */
    const roadmark_outlines_geom = new THREE.BufferGeometry();
    roadmark_outlines_geom.setAttribute('position', roadmarks_geom.attributes.position);
    roadmark_outlines_geom.setIndex(getStdVecEntries(odr_roadmarks_mesh.get_roadmark_outline_indices(), true));
    roadmark_outline_lines = new THREE.LineSegments(roadmark_outlines_geom, roadmark_outlines_material);
    roadmark_outline_lines.renderOrder = 8;
    roadmark_outline_lines.matrixAutoUpdate = false;
    disposable_objs.push(roadmark_outlines_geom);
    roadmark_outline_lines.visible = PARAMS.roadmarks;
    scene.add(roadmark_outline_lines);

    /* fit view and camera */
    if (refline_lines){
        const bbox_reflines = new THREE.Box3().setFromObject(refline_lines);
        max_diag_dist = bbox_reflines.min.distanceTo(bbox_reflines.max);
        if (max_diag_dist > 10000) {
            max_diag_dist = 2000;
        }
        // const max_diag_dist = 4700;
        camera.far = max_diag_dist * 1.5;
        controls.autoRotate = fit_view;
        if (fit_view)
            fitViewToBbox(bbox_reflines);
        /* ground grid */
        let bbox_center_pt = new THREE.Vector3();
        bbox_reflines.getCenter(bbox_center_pt);
        ground_grid = new THREE.GridHelper(max_diag_dist, max_diag_dist / 10, 0x2f2f2f, 0x2f2f2f);
        ground_grid.geometry.rotateX(Math.PI / 2);
        ground_grid.position.set(bbox_center_pt.x, bbox_center_pt.y, bbox_reflines.min.z - 0.1);
        disposable_objs.push(ground_grid.geometry);
        scene.add(ground_grid);
    

        /* fit light */
        light.position.set(bbox_reflines.min.x, bbox_reflines.min.y, bbox_reflines.max.z + max_diag_dist);
        light.target.position.set(bbox_center_pt.x, bbox_center_pt.y, bbox_center_pt.z);
        light.position.needsUpdate = true;
        light.target.position.needsUpdate = true;
        light.target.updateMatrixWorld();
    

        const t1 = performance.now();
        console.log("Heap size: " + ModuleOpenDrive.HEAP8.length / 1024 / 1024 + " mb");
        const info_msg = `
            <div class=popup_info>
            <h3>Finished loading</h3>
            <table>
                <tr><th>Time</th><th>${((t1 - t0) / 1e3).toFixed(2)}s</th></tr>

                <tr><th>Num Vertices</th><th>${renderer.info.render.triangles}</th></tr>
            </table>
            </div>`;
        notyf.open({ type : 'info', message : info_msg });
    }
    odr_roadmarks_mesh.delete();
    odr_lanes_mesh.delete();
    spotlight_info.style.display = "none";
    animate();
}

function animate()
{
    setTimeout(function() {
        requestAnimationFrame(animate);
    }, 1000 / 30);

    controls.update();

    if (PARAMS.spotlight && !spotlight_paused) {
        camera.setViewOffset(renderer.getContext().drawingBufferWidth, renderer.getContext().drawingBufferHeight, mouse.x * renderer.getPixelRatio() | 0, mouse.y * renderer.getPixelRatio() | 0, 1, 1);
        renderer.setRenderTarget(lane_picking_texture);
        renderer.render(lane_picking_scene, camera);
        renderer.setRenderTarget(roadmark_picking_texture);
        renderer.render(roadmark_picking_scene, camera);
        renderer.setRenderTarget(xyz_texture);
        renderer.render(xyz_scene, camera);
        renderer.setRenderTarget(st_texture);
        renderer.render(st_scene, camera);

        const lane_id_pixel_buffer = new Float32Array(4);
        renderer.readRenderTargetPixels(lane_picking_texture, 0, 0, 1, 1, lane_id_pixel_buffer);
        const roadmark_id_pixel_buffer = new Float32Array(4);
        renderer.readRenderTargetPixels(roadmark_picking_texture, 0, 0, 1, 1, roadmark_id_pixel_buffer);
        const xyz_pixel_buffer = new Float32Array(4);
        renderer.readRenderTargetPixels(xyz_texture, 0, 0, 1, 1, xyz_pixel_buffer);
        // xyz_pixel_buffer[0] += OpenDriveMap.x_offs;
        // xyz_pixel_buffer[1] += OpenDriveMap.y_offs;
        xyz_pixel_buffer[0] += 1500;
        xyz_pixel_buffer[1] += 4000;
        const st_pixel_buffer = new Float32Array(4);
        renderer.readRenderTargetPixels(st_texture, 0, 0, 1, 1, st_pixel_buffer);

        camera.clearViewOffset();
        renderer.setRenderTarget(null);

        if (isValid(lane_id_pixel_buffer)) {
            const decoded_lane_id = decodeUInt32(lane_id_pixel_buffer);
            const odr_lanes_mesh = road_network_mesh.userData.odr_road_network_mesh.lanes_mesh;
            if (INTERSECTED_LANE_ID != decoded_lane_id) {
                if (INTERSECTED_LANE_ID != 0xffffffff) {
                    const prev_lane_vert_idx_interval = odr_lanes_mesh.get_idx_interval_lane(INTERSECTED_LANE_ID);
                    road_network_mesh.geometry.attributes.color.array.fill(COLORS.road, prev_lane_vert_idx_interval[0] * 3, prev_lane_vert_idx_interval[1] * 3);
                }
                INTERSECTED_LANE_ID = decoded_lane_id;
                const lane_vert_idx_interval = odr_lanes_mesh.get_idx_interval_lane(INTERSECTED_LANE_ID);
                const vert_count = (lane_vert_idx_interval[1] - lane_vert_idx_interval[0]);
                applyVertexColors(road_network_mesh.geometry.attributes.color, new THREE.Color(COLORS.lane_highlight), lane_vert_idx_interval[0], vert_count);
                road_network_mesh.geometry.attributes.color.needsUpdate = true;
                spotlight_info.style.display = "block";
            }
            odr_lanes_mesh.delete();
        } else {
            if (INTERSECTED_LANE_ID != 0xffffffff) {
                const odr_lanes_mesh = road_network_mesh.userData.odr_road_network_mesh.lanes_mesh;
                const lane_vert_idx_interval = odr_lanes_mesh.get_idx_interval_lane(INTERSECTED_LANE_ID);
                road_network_mesh.geometry.attributes.color.array.fill(COLORS.road, lane_vert_idx_interval[0] * 3, lane_vert_idx_interval[1] * 3);
                road_network_mesh.geometry.attributes.color.needsUpdate = true;
                odr_lanes_mesh.delete();
            }
            INTERSECTED_LANE_ID = 0xffffffff;
            spotlight_info.style.display = "none";
        }

        if (isValid(roadmark_id_pixel_buffer)) {
            const decoded_roadmark_id = decodeUInt32(roadmark_id_pixel_buffer);
            const odr_roadmarks_mesh = road_network_mesh.userData.odr_road_network_mesh.roadmarks_mesh;
            if (INTERSECTED_ROADMARK_ID != decoded_roadmark_id) {
                if (INTERSECTED_ROADMARK_ID != 0xffffffff) {
                    const prev_roadmark_vert_idx_interval = odr_roadmarks_mesh.get_idx_interval_roadmark(INTERSECTED_ROADMARK_ID);
                    roadmarks_mesh.geometry.attributes.color.array.fill(COLORS.roadmark, prev_roadmark_vert_idx_interval[0] * 3, prev_roadmark_vert_idx_interval[1] * 3);
                }
                INTERSECTED_ROADMARK_ID = decoded_roadmark_id;
                const roadmark_vert_idx_interval = odr_roadmarks_mesh.get_idx_interval_roadmark(INTERSECTED_ROADMARK_ID);
                const vert_count = (roadmark_vert_idx_interval[1] - roadmark_vert_idx_interval[0]);
                applyVertexColors(roadmarks_mesh.geometry.attributes.color, new THREE.Color(COLORS.roadmark_highlight), roadmark_vert_idx_interval[0], vert_count);
                roadmarks_mesh.geometry.attributes.color.needsUpdate = true;
            }
            odr_roadmarks_mesh.delete();
        } else {
            if (INTERSECTED_ROADMARK_ID != 0xffffffff) {
                const odr_roadmarks_mesh = road_network_mesh.userData.odr_road_network_mesh.roadmarks_mesh;
                const roadmark_vert_idx_interval = odr_roadmarks_mesh.get_idx_interval_lane(INTERSECTED_ROADMARK_ID);
                roadmarks_mesh.geometry.attributes.color.array.fill(COLORS.roadmark, roadmark_vert_idx_interval[0] * 3, roadmark_vert_idx_interval[1] * 3);
                roadmarks_mesh.geometry.attributes.color.needsUpdate = true;
                odr_roadmarks_mesh.delete();
            }
            INTERSECTED_ROADMARK_ID = 0xffffffff;
        }

        if (INTERSECTED_LANE_ID != 0xffffffff) {
            const odr_lanes_mesh = road_network_mesh.userData.odr_road_network_mesh.lanes_mesh;
            const road_id = odr_lanes_mesh.get_road_id(INTERSECTED_LANE_ID);
            const lanesec_s0 = odr_lanes_mesh.get_lanesec_s0(INTERSECTED_LANE_ID);
            const lane_id = odr_lanes_mesh.get_lane_id(INTERSECTED_LANE_ID);
            // const lane_type = OpenDriveMap.roads.get(road_id).s_to_lanesection.get(lanesec_s0).id_to_lane.get(lane_id).type;
            odr_lanes_mesh.delete();
            // spotlight_info.innerHTML = `
            //         <table>
            //             <tr><th>road id</th><th>${road_id}</th></tr>
            //             <tr><th>section s0</th><th>${lanesec_s0.toFixed(2)}</th></tr>
            //             <tr><th>lane</th><th>${lane_id} <span style="color:gray;">${lane_type}</span></th></tr>
            //             <tr><th>s/t</th><th>[${st_pixel_buffer[0].toFixed(2)}, ${st_pixel_buffer[1].toFixed(2)}]</th>
            //             <tr><th>world</th><th>[${xyz_pixel_buffer[0].toFixed(2)}, ${xyz_pixel_buffer[1].toFixed(2)}, ${xyz_pixel_buffer[2].toFixed(2)}]</th></tr>
            //         </table>`;
        }
    }

    renderer.render(scene, camera);
}

function get_geometry(odr_meshunion)
{
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.Float32BufferAttribute(getStdVecEntries(odr_meshunion.vertices, true).flat(), 3));
    geom.setAttribute('st', new THREE.Float32BufferAttribute(getStdVecEntries(odr_meshunion.st_coordinates, true).flat(), 2));
    geom.setAttribute('color', new THREE.Float32BufferAttribute(new Float32Array(geom.attributes.position.count * 3), 3));
    geom.setAttribute('id', new THREE.Float32BufferAttribute(new Float32Array(geom.attributes.position.count * 4), 4));
    geom.setIndex(getStdVecEntries(odr_meshunion.indices, true));
    geom.computeVertexNormals();
    return geom;
}

function fitViewToBbox(bbox, restrict_zoom = true)
{
    let center_pt = new THREE.Vector3();
    bbox.getCenter(center_pt);

    const l2xy = 0.5 * Math.sqrt(Math.pow(bbox.max.x - bbox.min.x, 2.0) + Math.pow(bbox.max.y - bbox.min.y, 2));
    const fov2r = (camera.fov * 0.5) * (Math.PI / 180.0);
    const dz = l2xy / Math.tan(fov2r);

    camera.position.set(bbox.min.x, center_pt.y, bbox.max.z + dz);
    controls.target.set(center_pt.x, center_pt.y, center_pt.z);
    if (restrict_zoom)
        controls.maxDistance = center_pt.distanceTo(bbox.max) * 1.2;
    controls.update();
}

function fitViewToObj(obj)
{
    const bbox = new THREE.Box3().setFromObject(obj);
    fitViewToBbox(bbox);
}

function applyVertexColors(buffer_attribute, color, offset, count)
{
    const colors = new Float32Array(count * buffer_attribute.itemSize);
    for (let i = 0; i < (count * buffer_attribute.itemSize); i += buffer_attribute.itemSize) {
        colors[i] = color.r;
        colors[i + 1] = color.g;
        colors[i + 2] = color.b;
    }
    buffer_attribute.array.set(colors, offset * buffer_attribute.itemSize);
}

function getStdMapKeys(std_map, delete_map = false)
{
    let map_keys = [];
    const map_keys_vec = std_map.keys();
    for (let idx = 0; idx < map_keys_vec.size(); idx++)
        map_keys.push(map_keys_vec.get(idx));
    map_keys_vec.delete();
    if (delete_map)
        std_map.delete();
    return map_keys;
}

function getStdMapEntries(std_map)
{
    let map_entries = [];
    for (let key of getStdMapKeys(std_map))
        map_entries.push([ key, std_map.get(key) ]);
    return map_entries;
}

function getStdVecEntries(std_vec, delete_vec = false, ArrayType = null)
{
    let entries = ArrayType ? new ArrayType(std_vec.size()) : new Array(std_vec.size());
    for (let idx = 0; idx < std_vec.size(); idx++)
        entries[idx] = std_vec.get(idx);
    if (delete_vec)
        std_vec.delete();
    return entries;
}

function isValid(rgba)
{
    return !(rgba[0] == 1 && rgba[1] == 1 && rgba[2] == 1 && rgba[3] == 1);
}

function encodeUInt32(ui32)
{
    rgba = new Float32Array(4);
    rgba[0] = (Math.trunc(ui32) % 256) / 255.;
    rgba[1] = (Math.trunc(ui32 / 256) % 256) / 255.;
    rgba[2] = (Math.trunc(ui32 / 256 / 256) % 256) / 255.;
    rgba[3] = (Math.trunc(ui32 / 256 / 256 / 256) % 256) / 255.;
    return rgba;
}

function decodeUInt32(rgba)
{
    return Math.round(rgba[0] * 255) + Math.round(rgba[1] * 255) * 256 + Math.round(rgba[2] * 255) * 256 * 256 + Math.round(rgba[3] * 255) * 256 * 256 * 256;
}

function onWindowResize()
{
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
}

function onDocumentMouseMove(event)
{
    event.preventDefault();
    mouse.x = event.clientX;
    mouse.y = event.clientY;

}

function onDocumentMouseDbClick(e)
{
    if (INTERSECTED_LANE_ID != 0xffffffff) {
        const odr_lanes_mesh = road_network_mesh.userData.odr_road_network_mesh.lanes_mesh;
        const lane_vert_idx_interval = odr_lanes_mesh.get_idx_interval_lane(INTERSECTED_LANE_ID);
        const vertA = odr_lanes_mesh.vertices.get(lane_vert_idx_interval[0]);
        const vertB = odr_lanes_mesh.vertices.get(lane_vert_idx_interval[1] - 1);
        odr_lanes_mesh.delete();
        const bbox = new THREE.Box3();
        bbox.setFromArray([ vertA, vertB ].flat());
        fitViewToBbox(bbox, false);
    }
}