/* Animates an object hanging from a spring with damped oscillation.
   Given argument -t <seconds>, this generates a video file of length <seconds>.
   Otherwise the animation is shown at runtime.
*/

#include <kahto.h>
#include <unistd.h> // getopt

struct state {
	float height, velocity, mass;
	float neutral_height,
		  damping,
		  springconstant, // F = -kx; k = -F/x
		  gravity; // gravitational acceleration
	float endtime;
	double elapsed;
};

/* This function is only about physics and not useful for understanding
   how the kahto library works.
   This is a simple numerical solution, where energy is not conserved
   due to the approximation of constant acceleration between time steps.
   Therefore the oscillation amplitude increases, if damping = 0. */
float get_new_height(struct state *state, double elapsed) {
	float timediff = elapsed - state->elapsed;
	float timestep = 0.01;
	float used_time = 0;

	while (used_time+timestep <= timediff) {
loop:
		float disposition = state->height - state->neutral_height;
		float acceleration =
			-disposition * state->springconstant / state->mass
			+ state->gravity
			- state->damping * state->velocity;
		state->height += 0.5*acceleration*timestep*timestep + state->velocity*timestep;
		state->velocity += acceleration*timestep;
		used_time += timestep;
	}

	/* Go to the loop one more time with a smaller time step
	   to get a better match: used_time ≈ timediff */
	if (used_time + 1e-5 < timediff) {
		timestep = timediff - used_time;
		goto loop;
	}

	state->elapsed = elapsed;
	return state->height;
}

int update(struct kahto_figure *fig, uint32_t *canvas, int ystride, long count, double elapsed) {
	struct state *state = fig->userdata;
	if (state->endtime && elapsed > state->endtime)
		return -1;

	/* Update the height of the object */
	float height = get_new_height(state, elapsed);
	float *data = fig->graph[0]->data.list.ydata->data;
	data[0] = height;

	/* Adjust the y-axis range if necessary */
	struct kahto_axis *yaxis = kahto_yaxis0(fig);
	if (height < yaxis->min)
		yaxis->min = height;
	if (height > yaxis->max)
		yaxis->max = height;

	kahto_draw(fig, canvas, ystride);
	return 1;
}

int main(int argc, char **argv) {
	struct state state = {
		.springconstant = 15,
		.gravity = -9.81, // downwards
		.mass = 5,
		.damping = 0.1,
		.height = -0.5,
	};

	int opt;
	while ((opt = getopt(argc, argv, "t:")) >= 0)
		switch (opt) {
			case 't': state.endtime = atof(optarg); break;
		}

	struct kahto_figure *fig =
		kahto_y(&state.height, 1, // plot the object
			/* using the error bars to draw a line that represents the spring */
			.edata0=&state.neutral_height,
			.edata1=&state.neutral_height,
			.markerstyle.size=1./40);

	/* Range has to be set manually, since there is only 1 datum.
	   Automatic range would be from minimum to maximum, but those are equal in this case. */
	kahto_set_range(kahto_xaxis0(fig), -1, 1);
	kahto_set_range(kahto_yaxis0(fig), -5, 0.1);

	kahto_remove_ticks(kahto_xaxis0(fig));

	fig->update = update; // This function is responsible for updating the figure
	fig->userdata = &state; // This is where we can store our data to be used in the update function
	fig->wh[0] = 300;
	if (!state.endtime)
		kahto_show(fig);
	else
		kahto_write_mp4(fig, "animation.mp4", 30.0);
}
