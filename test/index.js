const addon = require( 'node-navmesh' );

let arr = [];
let a = null;
for (let i = 0; i < 1; ++i) {
	a = new addon.Navmesh();
	a.load( __dirname + '/map.bin' );
	arr.push( a );
}
var p = a.findRandomPoint();
console.log( p );
for (let i = 0; i < 10; ++i) {
	console.log( a.findRandomPoint( p, 200 ) );
}
var s = a.findNearestPoly( { x: -37128, y: 8826 + 50, z: -75515 }, { x: 100, y: 100, z: 100 } );
var e = a.findNearestPoly( { x: -37128, y: 8826, z: -75515 + 1000 }, { x: 100, y: 100, z: 100 } );
console.log( s );
console.log( e );
console.log( a.findStraightPath( s, e ) );
a.save( __dirname + '/map.obj' );
console.log( a );
// delete a;

setTimeout( () => {}, 100 );