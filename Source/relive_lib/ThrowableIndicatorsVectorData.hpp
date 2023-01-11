#pragma once

// static const double kNumber_0[17] = {
//     4,
//     -3, -4, 3, -4,
//     3, -3, 3, 3,
//     3, 4, -3, 4,
//     -3, 3, -3, -3};

// static const double kNumber_1[5] = {
//     1,
//     2, -4, 2, 4
//};

// static const double kNumber_2[21] = {
//     5,
//     -5, -4, 5, -4,
//     5, -3, 5, -1,
//     5, 0, -5, 0,
//     -5, 1, -5, 3,
//     -5, 4, 5, 4
// };

// static const double kNumber_3[17] = {
//     4,
//     -5,
//     -4,
//     5,
//     -4,
//     5,
//     -3,
//     5,
//     3,
//     5,
//     4,
//     -5,
//     4,
//     -4,
//     0,
//     4,
//     0,
// };

// static const double kNumber_4[13] = {
//     3,
//     -5, -4, -5, -1,
//     -5, 0, 4, 0,
//     5, -4, 5, 4};

// static const double kNumber_5[21] = {
//     5,
//     5, -4, -5, -4,
//     -5, -3, -5, -1,
//     -5, 0, 5, 0,
//     5, 1, 5, 3,
//     5, 4, -5, 4};

// static const double kNumber_6[21] = {
//     5,
//     5, -4, -5, -4, -5,
//     -3, -5, 3, -5, 4,
//     5, 4, 5, 3, 5,
//     1, 5, 0, -4, 0};

// static const double kNumber_7[9] = {
//     2,
//     -5, -4, 5, -4,
//     5, -3, 0, 4};

// static const double kNumber_8[21] = {
//     5,
//     -5, -4, 5, -4,
//     5, -3, 5, 3,
//     5, 4, -5, 4,
//     -5, 3, -5, -3,
//     -4, 0, 4, 0};

// static const double kNumber_9[17] = {
//     4,
//     5, 4, 5, -3,
//     5, -4, -5, -4,
//     -5, -3, -5, -1,
//     -5, 0, 4, 0};

// static const double kInfinity[25] = {
//     6,
//     -3, -2, -5, 0,
//     -5, 1, -3, 3,
//     -2, 3, 2, -2,
//     3, -2, 5, 0,
//     5, 1, 3, 3,
//     2, 3, -2, -2};

// static const double kCheckpoint[35] = {
//     8,
//     0, -6, 1, -6,
//     12, 0, 13, 0,
//     0, 6, 1, 6,
//     -11, 0, -12, 0,
//     2, -5, 11, -1, 11,
//     1, 2, 5, -1, 5,
//     -10, 1, -10, -1,
//     -1, -5, 0, 0,
// };

struct throwableVertex
{
    double x = 0;
    double y = 0;
};


throwableVertex posOffset = {0.5, 0.5};
double width = 10;
double height = 9;

throwableVertex bottomLeft = {posOffset.x, posOffset.y + height - 1};
throwableVertex bottomRight = {posOffset.x + width, posOffset.y + height - 1};

throwableVertex middleLeft = {posOffset.x, posOffset.y + height / 2 - 0.5};
throwableVertex middleRight = {posOffset.x + width, posOffset.y + height / 2 - 0.5};

throwableVertex topLeft = {posOffset.x, posOffset.y};
throwableVertex topRight = {posOffset.x + width, posOffset.y};

throwableVertex topMiddle = {posOffset.x + width / 2, posOffset.y};
throwableVertex bottomMiddle = {posOffset.x + width / 2, posOffset.y + height - 1};


static const double kNumber_0[17] = {
    4,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
    topLeft.x, topLeft.y, bottomLeft.x, bottomLeft.y,
    topRight.x, topRight.y, bottomRight.x, bottomRight.y
};

static const double kNumber_1[5] = {
    1,
    topMiddle.x, topMiddle.y, bottomMiddle.x, bottomMiddle.y
};

static const double kNumber_2[21] = {
    5,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    middleLeft.x, middleLeft.y, middleRight.x, middleRight.y,
    bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
    topRight.x, topRight.y, middleRight.x, middleRight.y,
    middleLeft.x, middleLeft.y, bottomLeft.x, bottomLeft.y
};

static const double kNumber_3[17] = {
    4,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    middleLeft.x + 2, middleLeft.y, middleRight.x - 2, middleRight.y,
    bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
    topRight.x, topRight.y, bottomRight.x, bottomRight.y,
};

static const double kNumber_4[13] = {
    3,
    topLeft.x, topLeft.y, middleLeft.x, middleLeft.y,
    middleLeft.x, middleLeft.y, middleRight.x - 2, middleRight.y,
    topRight.x, topRight.y, bottomRight.x, bottomRight.y
};

static const double kNumber_5[21] = {
    5,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    middleLeft.x, middleLeft.y, middleRight.x, middleRight.y,
    bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
    topLeft.x, topLeft.y, middleLeft.x, middleLeft.y,
    middleRight.x, middleRight.y, bottomRight.x, bottomRight.y
};

static const double kNumber_6[21] = {
    5,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    middleLeft.x + 2, middleLeft.y, middleRight.x, middleRight.y,
    bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
    topLeft.x, topLeft.y, bottomLeft.x, bottomLeft.y,
    middleRight.x, middleRight.y, bottomRight.x, bottomRight.y
};

static const double kNumber_7[9] = {
    2,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    topRight.x + 0.5, topLeft.y - 0.5, bottomMiddle.x, bottomMiddle.y //todo thinner line fn
};

static const double kNumber_8[21] = {
    5,
    topLeft.x, topLeft.y, topRight.x, topRight.y,
    middleLeft.x + 2, middleLeft.y, middleRight.x - 2, middleRight.y,
    bottomLeft.x, bottomLeft.y, bottomRight.x, bottomRight.y,
    topLeft.x, topLeft.y, bottomLeft.x, bottomLeft.y,
    topRight.x, topRight.y, bottomRight.x, bottomRight.y
};

static const double kNumber_9[17] = {
    4,
    topLeft.x, topLeft.y, middleLeft.x, middleLeft.y,
    middleLeft.x, middleLeft.y, middleRight.x - 2, middleRight.y,
    topRight.x, topRight.y, bottomRight.x, bottomRight.y,
    topLeft.x, topLeft.y, topRight.x, topRight.y
};

throwableVertex middleLeftLeft = { middleLeft.x - width / 2 - 2, middleLeft.y };
throwableVertex middleRightRight = { middleRight.x + width / 2 + 2, middleRight.y };
throwableVertex absoluteMiddle = { middleLeft.x + width / 2, middleLeft.y };

throwableVertex roundTopLeft = { middleLeftLeft.x + width / 2 / 3, middleLeftLeft.y + ( height / 2 ) * 2 / 3 - 0.5 };
throwableVertex roundBottomLeft = { middleLeftLeft.x + width / 2 / 3, middleLeftLeft.y - ( height / 2 ) * 2 / 3 + 0.5 };

throwableVertex roundBottomRight = { middleRightRight.x - width / 2 / 3 + 0.5, middleRightRight.y - ( height / 2 ) * 2 / 3 - 0.5 };
throwableVertex roundTopRight = { middleRightRight.x - width / 2 / 3 + 0.5, middleRightRight.y + ( height / 2 ) * 2 / 3 - 0.5 };

throwableVertex infTopLeft = { topLeft.x, topLeft.y - 1 };
throwableVertex infBottomLeft = { bottomLeft.x, bottomLeft.y + 1 };

throwableVertex infTopRight = { topRight.x + 2, topRight.y - 1 };
throwableVertex infBottomRight = { bottomRight.x + 2, bottomRight.y + 2 };

static const double kInfinity[49] = {
    12,
    infBottomLeft.x, infBottomLeft.y, roundTopLeft.x, roundTopLeft.y,
    roundTopLeft.x, roundTopLeft.y, middleLeftLeft.x, middleLeftLeft.y,
    middleLeftLeft.x, middleLeftLeft.y, roundBottomLeft.x, roundBottomLeft.y,
    roundBottomLeft.x, roundBottomLeft.y, infTopLeft.x - 0.5, infTopLeft.y,
    infBottomLeft.x - 1, infBottomLeft.y, absoluteMiddle.x + 1, absoluteMiddle.y,
    infTopLeft.x - 1, infTopLeft.y, absoluteMiddle.x + 1, absoluteMiddle.y,
    infBottomRight.x - 0.5, infBottomRight.y, absoluteMiddle.x + 0.5, absoluteMiddle.y,
    infTopRight.x - 0.5, infTopRight.y, absoluteMiddle.x + 0.5, absoluteMiddle.y,
    infBottomRight.x, infBottomRight.y, roundTopRight.x + 1, roundTopRight.y,
    roundTopRight.x, roundTopRight.y, middleRightRight.x + 1, middleRightRight.y,
    middleRightRight.x + 1, middleRightRight.y, roundBottomRight.x, roundBottomRight.y,
    roundBottomRight.x, roundBottomRight.y, infTopRight.x + 1, infTopRight.y
};

const double checkpointRatio = 240./640.;
const double checkpointHalfWidth = 13;
const double checkpointHalfHeight = 16 * checkpointRatio;
const double checkpointLeftX = 0;
const double checkpointLeftY = checkpointHalfHeight;
const double checkpointTopX = checkpointHalfWidth;
const double checkpointTopY = 0;
const double checkpointRightX = checkpointHalfWidth * 2;
const double checkpointRightY = checkpointHalfHeight;
const double checkpointBottomX = checkpointHalfWidth;
const double checkpointBottomY = checkpointHalfHeight * 2;

static const double kCheckpoint[17] = {
    4,
    checkpointBottomX - 1, checkpointBottomY + 0 * checkpointRatio, checkpointLeftX, checkpointLeftY,
    checkpointBottomX - 1, checkpointBottomY + 3 * checkpointRatio, checkpointRightX, checkpointRightY,
    checkpointRightX - 2, checkpointRightY, checkpointTopX - 1, checkpointTopY,
    checkpointTopX + 1, checkpointTopY, checkpointLeftX + 2, checkpointLeftY + 1 * checkpointRatio
};
