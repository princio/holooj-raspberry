


int draw_bbox(byte *image, int w, int h, box b) {

    int left  = (b.x-b.w/2.)*w;
    int right = (b.x+b.w/2.)*w;
    int top   = (b.y-b.h/2.)*h;
    int bot   = (b.y+b.h/2.)*h;

    byte color[3];
    color[0] = 250;
    color[1] = 50 + 50 * (i % 2);
    color[2] = 50 + 50 * (i % 3);

    int thickness = 0; //border width in pixel minus 1
    if(left < 0) left = thickness;
    if(right > w-thickness) right = w-thickness;
    if(top < 0) top = thickness;
    if(bot > h-thickness) bot = h-thickness;

    int top_row = 3*top*h;
    int bot_row = 3*bot*h;
    int left_col = 3*left;
    int right_col = 3*right;
    for(int k = left_col; k <= right_col; k+=3) {
        for(int wh = 0; wh <= thickness; wh++) {
            int border_line = wh*w*3;
            memcpy(&im[k + top_row - border_line], color, 3);
            memcpy(&im[k + bot_row + border_line], color, 3);
        }
    }
    for(int k = top_row; k <= bot_row; k+=3) {
        for(int wh = 0; wh < thickness; wh++) {
            memcpy(&im[k - wh + left_col],  color, 3);
            memcpy(&im[k + wh + right_col], color, 3);
        }
    }
}