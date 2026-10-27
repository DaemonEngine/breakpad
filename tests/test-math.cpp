
#include <cstdio>
#include <cmath>

double calculate_identity(double x)
{
	return std::sin(x) * std::sin(x) + std::cos(x) * std::cos(x);
}

double calculate_distance(double x1, double y1, double x2, double y2)
{
	double dx = x2 - x1;
	double dy = y2 - y1;

	return std::sqrt(dx * dx + dy * dy);
}

void print_result(const char* name, double result)
{
	printf("%-30s = %.6f\n", name, result);
}

int main()
{
	double a = 12.5;
	double b = 4.0;

	printf("=== Arithmetic ===\n\n");

	print_result("Addition", a + b);
	print_result("Subtraction", a - b);
	print_result("Multiplication", a * b);
	print_result("Division", a / b);

	printf("\n=== Math Functions ===\n\n");

	print_result("Square root", std::sqrt(a));
	print_result("Power", std::pow(b, 3));

	double angle = 0.75;

	print_result("Sinus", std::sin(angle));
	print_result("Cosinus", std::cos(angle));
	print_result("Pythagorean identity", calculate_identity(angle));

	printf("\n=== Geometry ===\n\n");

	print_result(
		"Distance",
		calculate_distance(1.0, 2.0, 4.0, 6.0)
	);

	return 0;
}
