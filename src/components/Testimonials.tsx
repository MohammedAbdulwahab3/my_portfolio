import { useEffect, useState } from "react";
import { Card } from "@/components/ui/card";
import { Avatar, AvatarFallback, AvatarImage } from "@/components/ui/avatar";
import { Star, Quote } from "lucide-react";
import { supabase } from "@/integrations/supabase/client";

interface Testimonial {
  id: string;
  name: string;
  position: string;
  company: string;
  content: string;
  avatar_url: string | null;
  rating: number;
  featured: boolean;
}

const Testimonials = () => {
  const [testimonials, setTestimonials] = useState<Testimonial[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchTestimonials = async () => {
      const { data, error } = await supabase
        .from("testimonials")
        .select("*")
        .order("featured", { ascending: false })
        .order("created_at", { ascending: false });

      if (!error && data) {
        setTestimonials(data);
      }
      setLoading(false);
    };

    fetchTestimonials();
  }, []);

  // Sample testimonials if none exist in database
  const sampleTestimonials = [
    {
      id: "1",
      name: "Sarah Johnson",
      position: "Product Manager",
      company: "TechCorp",
      content: "Outstanding work on our mobile app! The Flutter development was flawless and delivered ahead of schedule. Highly recommend for any mobile project.",
      avatar_url: null,
      rating: 5,
      featured: true,
    },
    {
      id: "2",
      name: "Michael Chen",
      position: "CTO",
      company: "StartupXYZ",
      content: "Excellent backend development with Django. The API architecture was clean, scalable, and well-documented. Great communication throughout the project.",
      avatar_url: null,
      rating: 5,
      featured: false,
    },
    {
      id: "3",
      name: "Emily Rodriguez",
      position: "Lead Designer",
      company: "DesignStudio",
      content: "Perfect implementation of our designs! The attention to detail and pixel-perfect execution exceeded our expectations. Will definitely work together again.",
      avatar_url: null,
      rating: 5,
      featured: false,
    },
  ];

  const displayTestimonials = testimonials.length > 0 ? testimonials : sampleTestimonials;

  if (loading) {
    return (
      <section className="py-20 px-4" id="testimonials">
        <div className="container mx-auto text-center">
          <p>Loading testimonials...</p>
        </div>
      </section>
    );
  }

  const renderStars = (rating: number) => {
    return Array.from({ length: 5 }, (_, i) => (
      <Star
        key={i}
        className={`h-4 w-4 ${
          i < rating ? "fill-yellow-400 text-yellow-400" : "text-gray-300"
        }`}
      />
    ));
  };

  const getInitials = (name: string) => {
    return name
      .split(" ")
      .map((n) => n[0])
      .join("")
      .toUpperCase();
  };

  return (
    <section className="py-20 px-4" id="testimonials">
      <div className="container mx-auto">
        <h2 className="text-4xl md:text-5xl font-bold text-center mb-4">
          What <span className="text-gradient">Clients Say</span>
        </h2>
        <p className="text-center text-muted-foreground mb-12 max-w-2xl mx-auto">
          Don't just take my word for it - here's what clients and colleagues have to say about working with me
        </p>

        {displayTestimonials.length === 0 ? (
          <Card className="p-12 text-center glass">
            <p className="text-muted-foreground">No testimonials yet. Check back soon!</p>
          </Card>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-8">
            {displayTestimonials.map((testimonial, index) => (
              <Card
                key={testimonial.id}
                className={`glass p-6 relative hover:shadow-lg transition-all duration-300 ${
                  testimonial.featured ? "ring-2 ring-primary/20" : ""
                }`}
                style={{
                  animationDelay: `${index * 100}ms`,
                  animationFillMode: "both",
                }}
              >
                {/* Quote Icon */}
                <Quote className="absolute top-4 right-4 h-6 w-6 text-primary/20" />
                
                {/* Rating */}
                <div className="flex gap-1 mb-4">
                  {renderStars(testimonial.rating)}
                </div>

                {/* Content */}
                <blockquote className="text-muted-foreground mb-6 italic">
                  "{testimonial.content}"
                </blockquote>

                {/* Author */}
                <div className="flex items-center gap-3">
                  <Avatar className="h-12 w-12">
                    <AvatarImage src={testimonial.avatar_url || undefined} />
                    <AvatarFallback className="bg-primary/10 text-primary">
                      {getInitials(testimonial.name)}
                    </AvatarFallback>
                  </Avatar>
                  <div>
                    <div className="font-semibold">{testimonial.name}</div>
                    <div className="text-sm text-muted-foreground">
                      {testimonial.position} at {testimonial.company}
                    </div>
                  </div>
                </div>

                {/* Featured Badge */}
                {testimonial.featured && (
                  <div className="absolute top-2 left-2">
                    <div className="bg-primary text-primary-foreground text-xs px-2 py-1 rounded-full">
                      Featured
                    </div>
                  </div>
                )}
              </Card>
            ))}
          </div>
        )}
      </div>
    </section>
  );
};

export default Testimonials;
